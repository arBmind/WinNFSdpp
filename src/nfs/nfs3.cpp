#include "nfs3.h"

#include "winfs/winfs_directory.h"
#include "wintime/wintime_convert.h"

#include "rpc/rpc.h"

#include "container/string_convert.h"

#define DEBUG_NFS_RPC

#ifdef DEBUG_NFS_RPC
#include <iostream>
#endif

#define GOOGLE_GLOG_DLL_DECL
#define GLOG_NO_ABBREVIATED_SEVERITIES
#include <glog/logging.h>

namespace nfs3
{
  namespace {
    struct filehandle_reader_t {
      filehandle_reader_t(const binary_reader_t& reader)
        : filehandle_m(xdr::opaque_reader<FILEHANDLE_SIZE>(reader, 0))
      {}

      size_t size() const { return filehandle_m.read_size(); }
      bool valid() const { return filehandle_m.valid(); }
      filehandle_t read() const { return filehandle_m.to_binary(); }

    private:
      xdr::opaque_reader_t filehandle_m;
    };

    struct dir_op_args_reader_t {
      dir_op_args_reader_t(const binary_reader_t& reader)
        : dir_m(reader)
        , name_m(xdr::opaque_reader(reader, dir_m.size()))
      {}
      size_t size() const { return dir_m.size() + name_m.read_size(); }
      bool valid() const { return dir_m.valid() && name_m.valid(); }

      dir_op_args_t read() const {
        dir_op_args_t result;
        result.directory = dir_m.read();
        result.name = name_m.to_string();
        return result;
      }

    private:
      filehandle_reader_t dir_m;
      xdr::opaque_reader_t name_m;
    };

    struct set_attr_reader_t {
      set_attr_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
      {}
      size_t size() const { return mtime_size(); }
      bool valid() const { return reader_m.has_size(size()); }

      bool has_mode() const { return reader_m.has_size(4) && reader_m.get32(0); }
      meta::optional_t<mode_t> mode() const {
        meta::optional_t<mode_t> result;
        if (has_mode()) result.set(reader_m.get32<mode_t>(4));
        return result;
      }
      uint32_t mode_size() const { return 4 + (has_mode() ? 4 : 0); }

      bool has_uid() const { return reader_m.has_size(mode_size() + 4) && reader_m.get32(mode_size()); }
      meta::optional_t<uid_t> uid() const {
        meta::optional_t<uid_t> result;
        if (has_mode()) result.set(reader_m.get32<uid_t>(4 + mode_size()));
        return result;
      }
      uint32_t uid_size() const { return mode_size() + 4 + (has_uid() ? 4 : 0); }

      bool has_gid() const { return reader_m.has_size(uid_size() + 4) && reader_m.get32(uid_size()); }
      meta::optional_t<gid_t> gid() const {
        meta::optional_t<gid_t> result;
        if (has_mode()) result.set(reader_m.get32<gid_t>(4 + uid_size()));
        return result;
      }
      uint32_t gid_size() const { return uid_size() + 4 + (has_gid() ? 4 : 0); }

      bool has_fsize() const { return reader_m.has_size(gid_size() + 4) && reader_m.get32(gid_size()); }
      meta::optional_t<size_t> fsize() const {
        meta::optional_t<size_t> result;
        if (has_mode()) result.set(reader_m.get64(4 + gid_size()));
        return result;
      }
      uint32_t fsize_size() const { return gid_size() + 4 + (has_fsize() ? 8 : 0); }

      time_how_t atime_how() const { return reader_m.has_size(fsize_size() + 4) ? reader_m.get32<time_how_t>(fsize_size()) : time_how_t::DONT_CHANGE; }
      time_t atime() const {
        time_t result;
        result.seconds = reader_m.get32(4 + fsize_size());
        result.nanoseconds = reader_m.get32(8 + fsize_size());
        return result;
      }
      uint32_t atime_size() const { return fsize_size() + 4 + (atime_how() == time_how_t::SET_TO_CLIENT_TIME ? 8 : 0); }

      time_how_t mtime_how() const { return reader_m.has_size(atime_size() + 4) ? reader_m.get32<time_how_t>(atime_size()) : time_how_t::DONT_CHANGE; }
      time_t mtime() const {
        time_t result;
        result.seconds = reader_m.get32(4 + atime_size());
        result.nanoseconds = reader_m.get32(8 + atime_size());
        return result;
      }
      uint32_t mtime_size() const { return atime_size() + 4 + (mtime_how() == time_how_t::SET_TO_CLIENT_TIME ? 8 : 0); }

      set_attr_t read() const {
        set_attr_t result;
        result.mode = mode();
        result.uid = uid();
        result.gid = gid();
        result.size = fsize();
        result.set_atime = atime_how();
        if (result.set_atime == time_how_t::SET_TO_CLIENT_TIME) result.atime = atime();
        result.set_mtime = mtime_how();
        if (result.set_mtime == time_how_t::SET_TO_CLIENT_TIME) result.mtime = mtime();
        return result;
      }

    private:
      const binary_reader_t reader_m;
    };

    struct read_dir_args_reader_t {
      read_dir_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , directory_handle_m(reader)
      {}
      size_t size() const { return 12 + directory_handle_m.size() + sizeof(cookie_verifier_t); }
      bool valid() const { return directory_handle_m.valid() && reader_m.has_size(size()); }

      read_dir_args_t read() const {
        read_dir_args_t result;
        result.directory = directory_handle_m.read();
        result.cookie = reader_m.get64(directory_handle_m.size());
        reader_m.get_binary(8 + directory_handle_m.size(), result.cookie_verifier);
        result.count = reader_m.get32(8 + directory_handle_m.size() + sizeof(cookie_verifier_t));
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t directory_handle_m;
    };

    struct read_dir_plus_args_reader_t {
      read_dir_plus_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , directory_handle_m(reader)
      {}
      size_t size() const { return 16 + directory_handle_m.size() + sizeof(cookie_verifier_t); }
      bool valid() const { return directory_handle_m.valid() && reader_m.has_size(size()); }

      read_dir_plus_args_t read() const {
        read_dir_plus_args_t result;
        result.directory = directory_handle_m.read();
        result.cookie = reader_m.get64(directory_handle_m.size());
        reader_m.get_binary(8 + directory_handle_m.size(), result.cookie_verifier);
        result.dircount = reader_m.get32(8 + directory_handle_m.size() + sizeof(cookie_verifier_t));
        result.maxcount = reader_m.get32(12 + directory_handle_m.size() + sizeof(cookie_verifier_t));
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t directory_handle_m;
    };

    struct commit_args_reader_t {
      commit_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , file_handle_m(reader)
      {}
      size_t size() const { return 12 + file_handle_m.size(); }
      bool valid() const { return file_handle_m.valid() && reader_m.has_size(size()); }

      commit_args_t read() const {
        commit_args_t result;
        result.file = file_handle_m.read();
        result.offset = reader_m.get64(0 + file_handle_m.size());
        result.count = reader_m.get32(8 + file_handle_m.size());
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t file_handle_m;
    };

    struct set_attr_args_reader_t {
      set_attr_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , directory_handle_m(reader)
        , set_attr_reader_m(reader.get_reader(directory_handle_m.size()))
      {}
      size_t size() const { return directory_handle_m.size() + set_attr_reader_m.size() + 4 + (has_time() ? 8 : 0); }
      bool valid() const { return directory_handle_m.valid() && set_attr_reader_m.valid() && reader_m.has_size(size()); }

      bool has_time() const {
        return reader_m.has_size(directory_handle_m.size() + set_attr_reader_m.size() + 4)
            && reader_m.get32(directory_handle_m.size() + set_attr_reader_m.size());
      }
      time_t check_time() const {
        time_t result;
        result.seconds = reader_m.get32(directory_handle_m.size() + set_attr_reader_m.size() + 4);
        result.nanoseconds = reader_m.get32(directory_handle_m.size() + set_attr_reader_m.size() + 8);
        return result;
      }

      set_attr_args_t read() const {
        set_attr_args_t result;
        result.filehandle = directory_handle_m.read();
        result.new_attributes = set_attr_reader_m.read();
        if (has_time()) result.check_time.set(check_time());
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t directory_handle_m;
      set_attr_reader_t set_attr_reader_m;
    };

    struct access_args_reader_t {
      access_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , directory_handle_m(reader)
      {}
      size_t size() const { return 4 + directory_handle_m.size(); }
      bool valid() const { return directory_handle_m.valid() && reader_m.has_size(size()); }

      access_args_t read() const {
        access_args_t result;
        result.filehandle = directory_handle_m.read();
        result.access = reader_m.get32(directory_handle_m.size());
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t directory_handle_m;
    };

    struct read_args_reader_t {
      read_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , file_handle_m(reader)
      {}
      size_t size() const { return file_handle_m.size() + 8 + 4; }
      bool valid() const { return file_handle_m.valid() && reader_m.has_size(size()); }

      read_args_t read() const {
        read_args_t result;
        result.filehandle = file_handle_m.read();
        result.offset = reader_m.get64(file_handle_m.size());
        result.count = reader_m.get32(8 + file_handle_m.size());
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t file_handle_m;
    };

    struct write_args_reader_t {
      write_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , file_handle_m(reader)
        , data_m(xdr::opaque_reader<>(reader, file_handle_m.size() + 8 + 4 + 4))
      {}
      size_t size() const { return file_handle_m.size() + 8 + 4 + 4 + data_m.read_size(); }
      bool valid() const { return file_handle_m.valid() && data_m.valid() && reader_m.has_size(size()); }

      write_args_t read() const {
        write_args_t result;
        result.filehandle = file_handle_m.read();
        result.offset = reader_m.get64(file_handle_m.size());
        result.count = reader_m.get32(8 + file_handle_m.size());
        result.stable = reader_m.get32<stable_how_t>(8 + 4 + file_handle_m.size());
        result.data = data_m.to_binary();
        return result;
      }

    private:
      binary_reader_t reader_m;
      filehandle_reader_t file_handle_m;
      xdr::opaque_reader_t data_m;
    };

    struct create_args_reader_t {
      create_args_reader_t(const binary_reader_t& reader)
        : reader_m(reader)
        , diropargs_m(reader)
        , set_attr_reader_m(reader.get_reader(diropargs_m.size() + 4))
      {}
      size_t size() const { return diropargs_m.size() + 4 + create_how_size(); }
      bool valid() const {
        return diropargs_m.valid()
            && reader_m.has_size(size())
            && (create_how() == create_how_t::EXCLUSIVE || set_attr_reader_m.valid());
      }

      create_how_t create_how() const {
        return reader_m.has_size(diropargs_m.size() + 4) ? reader_m.get32<create_how_t>(diropargs_m.size()) : create_how_t::UNCHECKED;
      }
      uint32_t create_how_size() const {
        return create_how() == create_how_t::EXCLUSIVE ? sizeof(create_verifier_t) : set_attr_reader_m.size();
      }

      create_args_t read() const {
        create_args_t result;
        result.where = diropargs_m.read();
        result.how = create_how();
        if (result.how == create_how_t::EXCLUSIVE) {
            create_verifier_t verifier;
            reader_m.get_binary(diropargs_m.size() + 4, verifier);
            result.verifier.set(verifier);
          }
        else {
            result.obj_attributes.set(set_attr_reader_m.read());
          }
        return result;
      }

    private:
      binary_reader_t reader_m;
      dir_op_args_reader_t diropargs_m;
      set_attr_reader_t set_attr_reader_m;
    };

    struct mkdir_args_reader_t {
      mkdir_args_reader_t(const binary_reader_t& reader)
        : diropargs_m(reader)
        , set_attr_reader_m(reader.get_reader(diropargs_m.size()))
      {}
      size_t size() const { return diropargs_m.size() + set_attr_reader_m.size(); }
      bool valid() const { return diropargs_m.valid() && set_attr_reader_m.valid(); }

      mkdir_args_t read() const {
        mkdir_args_t result;
        result.where = diropargs_m.read();
        result.attributes = set_attr_reader_m.read();
        return result;
      }

    private:
      dir_op_args_reader_t diropargs_m;
      set_attr_reader_t set_attr_reader_m;
    };

    struct rename_args_reader_t {
      rename_args_reader_t(const binary_reader_t& reader)
        : from_diropargs_m(reader)
        , to_diropargs_m(reader.get_reader(from_diropargs_m.size()))
      {}
      size_t size() const { return from_diropargs_m.size() + to_diropargs_m.size(); }
      bool valid() const { return from_diropargs_m.valid() && to_diropargs_m.valid(); }

      rename_args_t read() const {
        rename_args_t result;
        result.from = from_diropargs_m.read();
        result.to = to_diropargs_m.read();
        return result;
      }

    private:
      dir_op_args_reader_t from_diropargs_m;
      dir_op_args_reader_t to_diropargs_m;
    };

    void write_file_attr(binary_builder_t& builder, const file_attr_t& attr) {
      builder.append32(attr.type);
      builder.append32(attr.mode);
      builder.append32(attr.nlink);
      builder.append32(attr.uid);
      builder.append32(attr.gid);
      builder.append64(attr.size);
      builder.append64(attr.used);
      builder.append32(attr.rdev.data1);
      builder.append32(attr.rdev.data2);
      builder.append64(attr.fsid);
      builder.append64(attr.fileid);
      builder.append32(attr.atime.seconds);
      builder.append32(attr.atime.nanoseconds);
      builder.append32(attr.mtime.seconds);
      builder.append32(attr.mtime.nanoseconds);
      builder.append32(attr.ctime.seconds);
      builder.append32(attr.ctime.nanoseconds);
    }

    void write_wcc_attr(binary_builder_t& builder, const wcc_attr_t& attr) {
      builder.append64(attr.size);
      builder.append32(attr.mtime.seconds);
      builder.append32(attr.mtime.nanoseconds);
      builder.append32(attr.ctime.seconds);
      builder.append32(attr.ctime.nanoseconds);
    }

    void write_pre_op_attr(binary_builder_t& builder, const pre_op_attr_t& attr) {
      auto active = attr.is<wcc_attr_t>();
      builder.append32(active);
      if (active) write_wcc_attr(builder, attr.get<wcc_attr_t>());
    }

    void write_post_op_attr(binary_builder_t& builder, const post_op_attr_t& attr) {
      auto active = attr.is<file_attr_t>();
      builder.append32(active);
      if (active) write_file_attr(builder, attr.get<file_attr_t>());
    }

    void write_post_op_handle(binary_builder_t& builder, const post_op_filehandle_t& post_op) {
      auto active = post_op.is<filehandle_t>();
      builder.append32(active);
      if (active) xdr::write_opaque_binary<FILEHANDLE_SIZE>(builder, post_op.get<filehandle_t>());
    }

    void write_wcc_data(binary_builder_t& builder, const wcc_data_t& wcc_data) {
      write_pre_op_attr(builder, wcc_data.before);
      write_post_op_attr(builder, wcc_data.after);
    }

    binary_t write_get_attr_result(const get_attr_result_t& get_attr) {
      binary_builder_t builder;
      builder.append32(get_attr.status);
      if (get_attr.status == status_t::OK) {
          write_file_attr(builder, get_attr.attr);
        }
      return builder.build();
    }

    binary_t write_set_attr_result(const set_attr_result_t& set_attr) {
      binary_builder_t builder;
      builder.append32(set_attr.status);
      if (set_attr.status == status_t::OK) {
          write_wcc_data(builder, set_attr.wcc_data);
        }
      else {
          write_wcc_data(builder, set_attr.wcc_data);
        }
      return builder.build();
    }

    binary_t write_lookup_result(const lookup_result_t& lookup) {
      binary_builder_t builder;
      builder.append32(lookup.status);
      if (lookup.status == status_t::OK) {
          xdr::write_opaque_binary<FILEHANDLE_SIZE>(builder, lookup.object_handle);
          write_post_op_attr(builder, lookup.object_attributes);
          write_post_op_attr(builder, lookup.dir_attributes);
        }
      else {
          write_post_op_attr(builder, lookup.dir_attributes);
        }

      return builder.build();
    }

    binary_t write_access_result(const access_result_t& access) {
      binary_builder_t builder;
      builder.append32(access.status);
      if (access.status == status_t::OK) {
          write_post_op_attr(builder, access.obj_attributes);
          builder.append32(access.access);
        }
      else {
          write_post_op_attr(builder, access.obj_attributes);
        }

      return builder.build();
    }

    binary_t write_readlink_result(const readlink_result_t& readlink) {
      binary_builder_t builder;
      builder.append32(readlink.status);
      if (readlink.status == status_t::OK) {
          write_post_op_attr(builder, readlink.symlink_attributes);
          xdr::write_opaque_string(builder, readlink.path);
        }
      else {
          write_post_op_attr(builder, readlink.symlink_attributes);
        }
      return builder.build();
    }

    binary_t write_read_result(const read_result_t& read) {
      binary_builder_t builder;
      builder.append32(read.status);
      if (read.status == status_t::OK) {
          write_post_op_attr(builder, read.file_attributes);
          builder.append32(read.count);
          builder.append32(read.eof);
          xdr::write_opaque_binary(builder, read.data);
        }
      else {
          write_post_op_attr(builder, read.file_attributes);
        }
      return builder.build();
    }

    binary_t write_write_result(const write_result_t& write) {
      binary_builder_t builder;
      builder.append32(write.status);
      if (write.status == status_t::OK) {
          write_wcc_data(builder, write.file_wcc);
          builder.append32(write.count);
          builder.append32(write.committed);
          builder.append_binary(write.verifier);
        }
      else {
          write_wcc_data(builder, write.file_wcc);
        }
      return builder.build();
    }

    binary_t write_create_result(const create_result_t& create) {
      binary_builder_t builder;
      builder.append32(create.status);
      if (create.status == status_t::OK) {
          write_post_op_handle(builder, create.object);
          write_post_op_attr(builder, create.object_attributes);
          write_wcc_data(builder, create.directory_wcc);
        }
      else {
          write_wcc_data(builder, create.directory_wcc);
        }
      return builder.build();
    }

    binary_t write_remove_result(const remove_result_t& remove) {
      binary_builder_t builder;
      builder.append32(remove.status);
      if (remove.status == status_t::OK) {
          write_wcc_data(builder, remove.directory_wcc);
        }
      else {
          write_wcc_data(builder, remove.directory_wcc);
        }
      return builder.build();
    }

    binary_t write_rename_result(const rename_result_t& rename) {
      binary_builder_t builder;
      builder.append32(rename.status);
      if (rename.status == status_t::OK) {
          write_wcc_data(builder, rename.from_directory_wcc);
          write_wcc_data(builder, rename.to_directory_wcc);
        }
      else {
          write_wcc_data(builder, rename.from_directory_wcc);
          write_wcc_data(builder, rename.to_directory_wcc);
        }
      return builder.build();
    }

    binary_t write_read_dir_result(const read_dir_result_t& read_dir) {
      binary_builder_t builder;
      builder.append32(read_dir.status);
      if (read_dir.status == status_t::OK) {
          write_post_op_attr(builder, read_dir.directory_attributes);
          builder.append_binary(read_dir.cookie_verifier);
          xdr::write_list(builder, read_dir.reply, [](binary_builder_t& builder, const read_dir_entry_t& entry){
            builder.append64(entry.file_id);
            xdr::write_opaque_string<>(builder, entry.name);
            builder.append64(entry.cookie);
          }),
          builder.append32(read_dir.is_finished);
        }
      else {
          write_post_op_attr(builder, read_dir.directory_attributes);
        }

      return builder.build();
    }

    binary_t write_read_dir_plus_result(const read_dir_plus_result_t& read_dir_plus) {
      binary_builder_t builder;
      builder.append32(read_dir_plus.status);
      if (read_dir_plus.status == status_t::OK) {
          write_post_op_attr(builder, read_dir_plus.directory_attributes);
          builder.append_binary(read_dir_plus.cookie_verifier);
          xdr::write_list(builder, read_dir_plus.reply, [](binary_builder_t& builder, const read_dir_plus_entry_t& entry){
            builder.append64(entry.file_id);
            xdr::write_opaque_string<>(builder, entry.name);
            builder.append64(entry.cookie);
            write_post_op_attr(builder, entry.name_attributes);
            write_post_op_handle(builder, entry.name_handle);
          }),
          builder.append32(read_dir_plus.is_finished);
        }
      else {
          write_post_op_attr(builder, read_dir_plus.directory_attributes);
        }

      return builder.build();
    }

    binary_t write_fs_stat_result(const fs_stat_result_t& fs_stat) {
      binary_builder_t builder;
      builder.append32(fs_stat.status);
      if (fs_stat.status == status_t::OK) {
          write_post_op_attr(builder, fs_stat.object_attributes);
          builder.append64(fs_stat.total_bytes);
          builder.append64(fs_stat.free_bytes);
          builder.append64(fs_stat.available_bytes);
          builder.append64(fs_stat.total_files);
          builder.append64(fs_stat.free_files);
          builder.append64(fs_stat.available_files);
          builder.append32(fs_stat.invar_sec);
        }
      else {
          write_post_op_attr(builder, fs_stat.object_attributes);
        }

      return builder.build();
    }

    binary_t write_fs_info_result(const fs_info_result_t& fs_info) {
      binary_builder_t builder;
      builder.append32(fs_info.status);
      if (fs_info.status == status_t::OK) {
          write_post_op_attr(builder, fs_info.object_attributes);
          builder.append32(fs_info.read_max_size);
          builder.append32(fs_info.read_preferred_size);
          builder.append32(fs_info.read_suggested_multiple);
          builder.append32(fs_info.write_max_size);
          builder.append32(fs_info.write_preferred_size);
          builder.append32(fs_info.write_suggested_multiple);
          builder.append32(fs_info.directory_preferred_size);
          builder.append64(fs_info.file_maximum_size);
          builder.append32(fs_info.time_delta.seconds);
          builder.append32(fs_info.time_delta.nanoseconds);
          builder.append32(fs_info.properties);
        }
      else {
          write_post_op_attr(builder, fs_info.object_attributes);
        }

      return builder.build();
    }

    binary_t write_path_conf_result(const path_conf_result_t& path_conf) {
      binary_builder_t builder;
      builder.append32(path_conf.status);
      if (path_conf.status == status_t::OK) {
          write_post_op_attr(builder, path_conf.object_attributes);
          builder.append32(path_conf.link_max);
          builder.append32(path_conf.name_max);
          builder.append32(path_conf.no_truncation);
          builder.append32(path_conf.chown_restricted);
          builder.append32(path_conf.case_insensitive);
          builder.append32(path_conf.case_preserving);
        }
      else {
          write_post_op_attr(builder, path_conf.object_attributes);
        }

      return builder.build();
    }

    binary_t write_commit_result(const commit_result_t& commit) {
      binary_builder_t builder;
      builder.append32(commit.status);
      if (commit.status == status_t::OK) {
          write_wcc_data(builder, commit.file_wcc);
          builder.append_binary(commit.verifier);
        }
      else {
          write_wcc_data(builder, commit.file_wcc);
        }

      return builder.build();
    }

    inline filetype_t filetype_from_FileAttributes(DWORD FileAttributes) {
      if (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return filetype_t::SYMLINK;
      if (FileAttributes & FILE_ATTRIBUTE_DIRECTORY) return filetype_t::DIRECTORY;
      //if (FileAttributes & FILE_ATTRIBUTE_NORMAL) {
      return filetype_t::REGULAR_FILE;
    }

    inline mode_t mode_from_FileAttributes(DWORD FileAttributes) {
      if (FileAttributes & FILE_ATTRIBUTE_SYSTEM) return 0; // nothing allowed
      //if (FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) return 0; // nothing allowed
      mode_t result =
          OWNER_READ | OWNER_WRITE | OWNER_EXECUTE |
          GROUP_READ | GROUP_WRITE | GROUP_EXECUTE |
          OTHERS_READ | OTHERS_WRITE | OTHERS_EXECUTE;
      if (FileAttributes & FILE_ATTRIBUTE_READONLY) {
          result &= ~(OWNER_WRITE | GROUP_WRITE | OTHERS_WRITE);
        }
      return result;
    }

    inline wcc_attr_t wcc_attr_from_BASIC_and_STANDARD_INFO(const FILE_BASIC_INFO& basic_info, const FILE_STANDARD_INFO& standard_info) {
      wcc_attr_t result;
      result.size = standard_info.EndOfFile.QuadPart;
      DLOG(INFO) << "FROM size: " << result.size ;
      result.mtime = wintime::convert_LARGE_INTEGER_to_unix_time(basic_info.LastWriteTime);
      result.ctime = wintime::convert_LARGE_INTEGER_to_unix_time(basic_info.ChangeTime);
      return result;
    }

    inline file_attr_t file_attr_from_BASIC_and_STANDARD_INFO(const FILE_BASIC_INFO& basic_info, const FILE_STANDARD_INFO& standard_info, const winfs::volume_file_id_t& id) {
      file_attr_t result;
      result.type = filetype_from_FileAttributes(basic_info.FileAttributes);
      result.mode = mode_from_FileAttributes(basic_info.FileAttributes);
      result.nlink = standard_info.NumberOfLinks;
      result.uid = 0; // TODO: make this configurable
      result.gid = 0;
      result.size = standard_info.EndOfFile.QuadPart;
      result.used = standard_info.AllocationSize.QuadPart;
      DLOG(INFO) << "TO size: " << result.size << " used: " << result.used ;
      result.fsid = id.VolumeSerialNumber;
      result.fileid = *reinterpret_cast<const uint64_t*>(&id.FileId); // filehandle_view.volume_file_id.FileId; - too large
      result.atime = wintime::convert_LARGE_INTEGER_to_unix_time(basic_info.LastAccessTime);
      result.mtime = wintime::convert_LARGE_INTEGER_to_unix_time(basic_info.LastWriteTime);
      result.ctime = wintime::convert_LARGE_INTEGER_to_unix_time(basic_info.ChangeTime);
      return result;
    }

    inline meta::optional_t<file_attr_t> file_attr_from_object(const winfs::unique_object_t& file, const winfs::volume_file_id_t& id) {
      FILE_BASIC_INFO basic_info;
      bool success = file.basic_info(basic_info);
      if (!success) return {};

      FILE_STANDARD_INFO standard_info;
      success = file.standard_info(standard_info);
      if (!success) return {};

      return file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, id);
    }

  } // namespace

  rpc_program::rpc_program(const mount_cache_t &mount_cache)
    : mount_cache_m(mount_cache)
  {
    ::GetSystemTimeAsFileTime(reinterpret_cast<FILETIME*>(&cookie_verifier_m));
  }

  get_attr_result_t rpc_program::get_attr(const filehandle_t& filehandle)
  {
    DLOG(INFO) << "Get Attr..." ;
    get_attr_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    auto attr = file_attr_from_object(file, filehandle_view.volume_file_id);
    if (attr.empty()) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.attr = attr.get<file_attr_t>();
    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath()) ;
    return result;
  }

  set_attr_result_t rpc_program::set_attr(const set_attr_args_t& args)
  {
    DLOG(INFO) << "Set Attr..." ;
    set_attr_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES | FILE_APPEND_DATA>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    FILE_STANDARD_INFO standard_info;
    success = file.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    auto attr = wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info);
    result.wcc_data.before.set(attr);

    if (args.check_time.is<time_t>()) {
        if (attr.ctime != args.check_time.get<time_t>()) {
            result.status = status_t::ERR_NOT_SYNC;
            result.wcc_data.after = file_attr_from_object(file, filehandle_view.volume_file_id);
            return result;
          }
      }

    if (args.new_attributes.size.is<size_t>() && !standard_info.Directory) {
        success = file.set_size(args.new_attributes.size.get<size_t>());
        if (!success) {
            result.status = status_t::ERR_INVAL;
            result.wcc_data.after = file_attr_from_object(file, filehandle_view.volume_file_id);
            return result;
          }
      }

    bool set_basic_info = false;
    if (args.new_attributes.set_atime == time_how_t::SET_TO_SERVER_TIME) {
        FILETIME filetime;
        ::GetSystemTimeAsFileTime(&filetime);
        basic_info.LastAccessTime.HighPart = filetime.dwHighDateTime;
        basic_info.LastAccessTime.LowPart = filetime.dwLowDateTime;
        set_basic_info = true;
      }
    if (args.new_attributes.set_mtime == time_how_t::SET_TO_SERVER_TIME) {
        FILETIME filetime;
        ::GetSystemTimeAsFileTime(&filetime);
        basic_info.LastWriteTime.HighPart = filetime.dwHighDateTime;
        basic_info.LastWriteTime.LowPart = filetime.dwLowDateTime;
        set_basic_info = true;
      }
    if (set_basic_info) {
        success = file.set_basic_info(basic_info);
        if (!success) {
            result.status = status_t::ERR_INVAL;
            result.wcc_data.after = file_attr_from_object(file, filehandle_view.volume_file_id);
            return result;
          }
      }

    result.wcc_data.after = file_attr_from_object(file, filehandle_view.volume_file_id);

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath()) ;
    return result;

  }

  lookup_result_t rpc_program::lookup(const dir_op_args_t& args)
  {
    DLOG(INFO) << "Lookup... " << args.name ;
    lookup_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    FILE_STANDARD_INFO standard_info;
    success = file.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.dir_attributes.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));

    if (0 == (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        result.status = status_t::ERR_NOTDIR;
        return result;
      }

    if ("." == args.name) {
        result.object_attributes = result.dir_attributes;
        result.object_handle = args.directory;
        result.status = status_t::OK;
        return result;
      }
    if (std::any_of(args.name.begin(), args.name.end(), [](char chr) { return chr == '/' || chr == '\\'; })) {
        result.status = status_t::ERR_INVAL;
        return result;
      }

    auto filename = convert::to_wstring(args.name);
    auto lookup_file = file.lookup<FILE_READ_ATTRIBUTES>(filename);
    if (!lookup_file.valid()) {
        result.status = status_t::ERR_NO_ENTRY;
        return result;
      }

    winfs::volume_file_id_t lookup_id;
    success = lookup_file.id(lookup_id);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    auto lookup_attr = file_attr_from_object(lookup_file, lookup_id);
    if (lookup_attr.empty()) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.object_attributes = lookup_attr;

    auto& lookup_filehandle = mount_filehandle_t::create_in_binary(result.object_handle);
    lookup_filehandle.mount_id = filehandle_view.mount_id;
    lookup_filehandle.volume_file_id = lookup_id;

    result.status = status_t::OK;
    DLOG(INFO) << "LOOKUP retunred " << binary_reader_t::binary(result.object_handle).get_hex_string(0,sizeof(result.object_handle))
               <<  " for directory" << convert::to_string(lookup_file.fullpath());
    return result;
  }

  access_result_t rpc_program::access(const access_args_t& args)
  {
    DLOG(INFO) << "Access..." ;
    access_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = file.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.obj_attributes.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));

    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_SYSTEM)) {
        result.access = 0; // do not allow anything with reparse points
      }
    else {
        result.access = args.access;
        if (basic_info.FileAttributes & FILE_ATTRIBUTE_READONLY) {
            result.access &= ~(ACCESS_MODIFY | ACCESS_EXTEND);
          }
        if (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            result.access &= ~(ACCESS_EXECUTE);
          }
        else result.access &= ~(ACCESS_LOOKUP | ACCESS_DELETE);
      }

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath()) ;
    return result;
  }

  readlink_result_t rpc_program::readlink(const filehandle_t& filehandle)
  {
    DLOG(INFO) << "Readlink..." ;
    readlink_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    result.symlink_attributes = file_attr_from_object(file, filehandle_view.volume_file_id);
    if (result.symlink_attributes.empty()) {
        result.status = status_t::ERR_IO;
        return result;
      }

    auto symlink_path = std::wstring(); //TODO: file.target_path();
    result.path = convert::to_string(symlink_path);
    std::transform(result.path.begin(), result.path.end(), result.path.begin(), [](char chr) {
        if (chr == '\\') return '/';
        return chr;
      });

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath()) ;
    return result;
  }

  read_result_t rpc_program::read(const read_args_t& args)
  {
    DLOG(INFO) << "Read..." ;
    read_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto object = mount_directory.by_id<FILE_READ_ATTRIBUTES|FILE_READ_DATA>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.file_attributes.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));

    if (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        result.status = status_t::ERR_ISDIR;
        return result;
      }

    auto file = object.as_file();
    success = file.seek(args.offset);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.data.resize(args.count);
    success = file.read(result.data);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    result.count = result.data.size();
    result.eof = (args.count != 0 && result.data.empty())
        || (args.offset + result.data.size() == (uint64_t)standard_info.EndOfFile.QuadPart);

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(object.fullpath()) ;
    return result;
  }

  write_result_t rpc_program::write(const write_args_t& args)
  {
    DLOG(INFO) << "Write..." ;
    write_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if (!mount_pair.first.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto object = /*args.stable != stable_how_t::UNSTABLE
        ? mount_directory.by_id<FILE_READ_ATTRIBUTES | FILE_GENERIC_WRITE, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, 0>(filehandle_view.volume_file_id.FileId)
        :*/ mount_directory.by_id<FILE_READ_ATTRIBUTES | FILE_GENERIC_WRITE, 0, 0>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        DLOG(INFO) << "Failed Open: " << GetLastError() ;
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.file_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        result.status = status_t::ERR_INVAL;
        return result;
      }

    auto file = object.as_file();
    if (0 == args.offset) {
        success = file.truncate();
      }
    else {
        success = file.seek(args.offset);
      }
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    //auto count = std::min(args.count, args.data.size());
    //if (count < args.data.size()) args.data.resize(args.count);

    success = file.write(args.data);
    if (!success) {
        DLOG(INFO) << "Failed Write: " << GetLastError() ;
        result.status = status_t::ERR_IO;
        return result;
      }

    // file is modified - no fail allowed
    success = object.basic_info(basic_info);
    success &= object.standard_info(standard_info);
    if (success) {
        result.file_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
      }

    result.count = args.count;
    result.committed = args.stable != stable_how_t::UNSTABLE ? stable_how_t::FILE_SYNC : stable_how_t::UNSTABLE;
    result.verifier = cookie_verifier_m;

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(object.fullpath()) ;
    return result;
  }

  create_result_t rpc_program::create(const create_args_t& args)
  {
    DLOG(INFO) << "Create..." ;
    create_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.where.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    // no support for exclusive
    if (args.how == create_how_t::EXCLUSIVE) {
        result.status = status_t::ERR_NOTSUPP;
        return result;
      }

    auto object = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }

    auto dirpath = object.fullpath();
    auto filepath = dirpath + L'\\' + convert::to_wstring(args.where.name);

    auto file = winfs::create_file<FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES>(filepath);
    if (!file.valid()) {
        result.status = status_t::ERR_IO;
        result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);
        return result;
      }

    result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);

    filehandle_t created_filehandle;
    auto& created_filehandle_view = mount_filehandle_t::create_in_binary(created_filehandle);
    created_filehandle_view.mount_id = filehandle_view.mount_id;
    file.id(created_filehandle_view.volume_file_id);

    result.object_attributes = file_attr_from_object(file, created_filehandle_view.volume_file_id);
    result.object.set(created_filehandle);

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath()) ;
    return result;
  }

  mkdir_result_t rpc_program::mkdir(const mkdir_args_t& args)
  {
    mkdir_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.where.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto object = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }

    auto dirpath = object.fullpath();
    auto filepath = dirpath + L'\\' + convert::to_wstring(args.where.name);

    uint32_t create_success = winfs::directory_t::create(filepath);

    result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);

    if (create_success) {
        assert(create_success == 183);
        result.status = status_t::ERR_EXIST;//Thats the one
        return result;
      }
    auto target = winfs::open_path<FILE_READ_ATTRIBUTES>(filepath);
    if (!target.valid()) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);

    filehandle_t created_filehandle;
    auto& created_filehandle_view = mount_filehandle_t::create_in_binary(created_filehandle);
    created_filehandle_view.mount_id = filehandle_view.mount_id;
    target.id(created_filehandle_view.volume_file_id);

    result.object_attributes = file_attr_from_object(target, created_filehandle_view.volume_file_id);
    result.object.set(created_filehandle);

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(object.fullpath()) ;
    return result;
  }

  remove_result_t rpc_program::remove(const dir_op_args_t& args)
  {
    DLOG(INFO) << "Remove..." ;
    remove_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto object = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }

    auto dirpath = object.fullpath();
    auto filepath = dirpath + L'\\' + convert::to_wstring(args.name);

    success = winfs::file_t::remove(filepath);

    result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);

    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(object.fullpath()) ;
    return result;
  }

  rmdir_result_t rpc_program::rmdir(const dir_op_args_t& args)
  {
    DLOG(INFO) << "RmDir..." ;
    rmdir_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto object = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));
        return result;
      }

    auto dirpath = object.fullpath();
    auto filepath = dirpath + L'\\' + convert::to_wstring(args.name);

    success = winfs::directory_t::remove(filepath);

    result.directory_wcc.after = file_attr_from_object(object, filehandle_view.volume_file_id);

    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(object.fullpath()) ;
    return result;
  }

  rename_result_t rpc_program::rename(const rename_args_t& args)
  {
    DLOG(INFO) << "Rename... " << args.from.name << " to " << args.to.name ;
    rename_result_t result;

    // build from data
    const auto& from_filehandle_view = mount_filehandle_t::view_binary(args.from.directory);
    auto from_mount_pair = mount_cache_m.get(from_filehandle_view.mount_id);
    auto from_mount_directory = from_mount_pair.first;
    if ( !from_mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto from_mount_filehandle = mount_filehandle_t::view_binary(from_mount_pair.second);
    if ( from_mount_filehandle.volume_file_id.VolumeSerialNumber != from_filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto from_object = from_mount_directory.by_id<FILE_READ_ATTRIBUTES>(from_filehandle_view.volume_file_id.FileId);
    if (!from_object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = from_object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = from_object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.from_directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.from_directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, from_filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.from_directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, from_filehandle_view.volume_file_id));
        return result;
      }

    auto from_dirpath = from_object.fullpath();
    auto from_filepath = from_dirpath + L'\\' + convert::to_wstring(args.from.name);

    // build to data
    const auto& to_filehandle_view = mount_filehandle_t::view_binary(args.to.directory);
    auto to_mount_pair = mount_cache_m.get(to_filehandle_view.mount_id);
    auto to_mount_directory = to_mount_pair.first;
    if ( !to_mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto to_mount_filehandle = mount_filehandle_t::view_binary(to_mount_pair.second);
    if ( to_mount_filehandle.volume_file_id.VolumeSerialNumber != to_filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto to_object = to_mount_directory.by_id<FILE_READ_ATTRIBUTES>(to_filehandle_view.volume_file_id.FileId);
    if (!to_object.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    success = to_object.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    success = to_object.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.to_directory_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));

    if (!standard_info.Directory) {
        result.status = status_t::ERR_NOTDIR;
        result.to_directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, to_filehandle_view.volume_file_id));
        return result;
      }
    if (basic_info.FileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)) {
        result.status = status_t::ERR_ACCESS;
        result.to_directory_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, to_filehandle_view.volume_file_id));
        return result;
      }

    auto to_dirpath = to_object.fullpath();
    auto to_filepath = to_dirpath + L'\\' + convert::to_wstring(args.to.name);

    success = winfs::file_t::move(from_filepath, to_filepath);

    result.from_directory_wcc.after = file_attr_from_object(from_object, from_filehandle_view.volume_file_id);
    result.to_directory_wcc.after = file_attr_from_object(to_object, to_filehandle_view.volume_file_id);

    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.status = status_t::OK;
    DLOG(INFO) << "...success " << convert::to_string(from_object.fullpath()) << " to " << convert::to_string(to_object.fullpath()) ;
    return result;
  }

  read_dir_result_t rpc_program::read_dir(const read_dir_args_t& args)
  {
    DLOG(INFO) << "Read Dir..." ;
    read_dir_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    // take care of verifier cookie
    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    const int64_t& args_verifier = *reinterpret_cast<const int64_t*>(&args.cookie_verifier[0]);
    if (0 != args_verifier && args_verifier != basic_info.LastWriteTime.QuadPart) {
        result.status = status_t::ERR_BAD_COOKIE;
        return result;
      }
    *reinterpret_cast<int64_t*>(&result.cookie_verifier[0]) = basic_info.LastWriteTime.QuadPart;

    // setup
    uint64_t fileCookie = 0;
    convert::state_t mbstate;

    size_t totalcount = 4 + // dir_attributes
        sizeof(cookie_verifier_t) +
        4 + // reply terminator
        4; // eof

    result.is_finished = true;
    success = file.as_directory().enumerate([&](const winfs::directory_entry_t& entry) {
        ++fileCookie;
        if (fileCookie <= args.cookie) return true; // skip already read parts

        auto entry_filename = entry.filename();
        auto entry_filename_mbslen = convert::to_string_length(entry_filename, mbstate);

        totalcount +=
            4 + // marker
            8 + // file_id
            4 + entry_filename_mbslen + // filename
            8; // cookie
        if (totalcount > args.count) {
            result.is_finished = false;
            return false;
          }
        DLOG(INFO) << convert::to_string(entry_filename) ;

        read_dir_entry_t result_entry;
        result_entry.file_id = 1;
        result_entry.name = convert::to_string_with_length(entry_filename, entry_filename_mbslen, mbstate);
        result_entry.cookie = fileCookie;

        result.reply.push_back(result_entry);
        return true;
      });
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    else
      DLOG(INFO) << "...success " << convert::to_string(file.fullpath());

    return result;
  }

  read_dir_plus_result_t rpc_program::read_dir_plus(const read_dir_plus_args_t& args)
  {
    DLOG(INFO) << "Read Dir Plus..." ;
    read_dir_plus_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(args.directory);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    auto file = mount_directory.by_id<FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    // take care of verifier cookie
    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    const int64_t& args_verifier = *reinterpret_cast<const int64_t*>(&args.cookie_verifier[0]);
    if (0 != args_verifier && args_verifier != basic_info.LastWriteTime.QuadPart) {
        result.status = status_t::ERR_BAD_COOKIE;
        return result;
      }
    *reinterpret_cast<int64_t*>(&result.cookie_verifier[0]) = basic_info.LastWriteTime.QuadPart;

    // setup
    uint64_t fileCookie = 0;
    convert::state_t mbstate;

    size_t dircount = 0;
    size_t totalcount = 4 + // dir_attributes
        sizeof(cookie_verifier_t) +
        4 + // reply terminator
        4; // eof

    result.is_finished = true;
    success = file.as_directory().enumerate([&](const winfs::directory_entry_t& entry) {
        ++fileCookie;
        if (fileCookie <= args.cookie) return true; // skip already read parts

        auto entry_filename = entry.filename();
        auto entry_filename_mbslen = convert::to_string_length(entry_filename, mbstate);

        dircount +=
            8 + // file_id
            4 + entry_filename_mbslen + // filename
            8; // cookie
        if (dircount > args.dircount) {
            result.is_finished = false;
            return false;
          }

        totalcount +=
            4 + // marker
            8 + // file_id
            4 + entry_filename_mbslen + // filename
            8 + // cookie
            4 + sizeof(file_attr_t) + // name_attributes
            4 + sizeof(winfs::file_id_t); // name_handle
        if (totalcount > args.maxcount) {
            result.is_finished = false;
            return false;
          }
        DLOG(INFO) << convert::to_string(entry_filename);

        read_dir_plus_entry_t result_entry;
        auto entry_id = entry.id();
        result_entry.file_id = *reinterpret_cast<const uint64_t*>(&entry_id);
        result_entry.name = convert::to_string_with_length(entry_filename, entry_filename_mbslen, mbstate);
        result_entry.cookie = fileCookie;

        file_attr_t entry_attr;
        entry_attr.type = filetype_from_FileAttributes(entry.attributes());
        entry_attr.mode = mode_from_FileAttributes(entry.attributes());
        entry_attr.nlink = 1;
        entry_attr.uid = 0; // TODO: make this configurable
        entry_attr.gid = 0;
        entry_attr.size = entry.size();
        entry_attr.used = entry.allocated_size();
        entry_attr.fsid = filehandle_view.volume_file_id.VolumeSerialNumber;
        entry_attr.fileid = *reinterpret_cast<const uint64_t*>(&entry_id);
        entry_attr.atime = wintime::convert_LARGE_INTEGER_to_unix_time(entry.lastAccessTime());
        entry_attr.mtime = wintime::convert_LARGE_INTEGER_to_unix_time(entry.lastWriteTime());
        entry_attr.ctime = wintime::convert_LARGE_INTEGER_to_unix_time(entry.changeTime());
        result_entry.name_attributes.set(entry_attr);

        filehandle_t entry_handle;
        auto& entry_filehandle_view = mount_filehandle_t::create_in_binary(entry_handle);
        entry_filehandle_view.mount_id = filehandle_view.mount_id;
        entry_filehandle_view.volume_file_id.VolumeSerialNumber = filehandle_view.volume_file_id.VolumeSerialNumber;
        entry_filehandle_view.volume_file_id.FileId = entry.id();
        result_entry.name_handle.set(entry_handle);

        result.reply.push_back(result_entry);
        return true;
      });
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    else
      DLOG(INFO) << "...success " << convert::to_string(file.fullpath());

    result.status = status_t::OK;
    return result;
  }

  fs_stat_result_t rpc_program::fs_stat(const filehandle_t &root)
  {
    DLOG(INFO) << "FS stat..." ;
    fs_stat_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(root);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber
         || 0 != memcpy_s(&mount_filehandle.volume_file_id.FileId, sizeof(mount_filehandle.volume_file_id.FileId),
                          &filehandle_view.volume_file_id.FileId, sizeof(filehandle_view.volume_file_id.FileId))) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // wrong volume
      }

    //result.directory_attributes.file_attr.set();

    auto file = mount_directory.by_id<>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }
    auto fullpath = file.fullpath();

    ULARGE_INTEGER available_bytes, total_bytes, free_bytes;
    bool success = ::GetDiskFreeSpaceEx(fullpath.c_str(), &available_bytes, &total_bytes, &free_bytes);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.status = status_t::OK;
    result.total_bytes = total_bytes.QuadPart;
    result.free_bytes = free_bytes.QuadPart;
    result.available_bytes = available_bytes.QuadPart;
    result.total_files = 1ull << (32 + 1);
    result.free_files = 1ull << 32;
    result.available_files = 1ull << 32;
    result.invar_sec = 0;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath());
    return result;
  }

  fs_info_result_t rpc_program::fs_info(const filehandle_t& root)
  {
    DLOG(INFO) << "FS info..." ;
    fs_info_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(root);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if ( mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber
         || 0 != memcpy_s(&mount_filehandle.volume_file_id.FileId, sizeof(mount_filehandle.volume_file_id.FileId),
                          &filehandle_view.volume_file_id.FileId, sizeof(filehandle_view.volume_file_id.FileId))) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // not the mount directly
      }

    auto file = mount_directory.by_id<FILE_READ_ATTRIBUTES>(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    result.object_attributes = file_attr_from_object(file, filehandle_view.volume_file_id);
    if (result.object_attributes.empty()) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.status = status_t::OK;
    // TODO: query filesystem info of windows

    DLOG(INFO) << "...success " << convert::to_string(file.fullpath());

    return result;
  }

  path_conf_result_t rpc_program::path_conf(const filehandle_t& filehandle)
  {
    DLOG(INFO) << "Path Conf..." ;

    path_conf_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(filehandle);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if (mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // not the mount directly
      }

    auto file = mount_directory.by_id(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    result.status = status_t::OK;
    // see defaults
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath());

    return result;
  }

  commit_result_t rpc_program::commit(const commit_args_t& commit)
  {
    DLOG(INFO) << "Commit... offset: " << commit.offset << " count: " << commit.count ;
    commit_result_t result;

    const auto& filehandle_view = mount_filehandle_t::view_binary(commit.file);
    auto mount_pair = mount_cache_m.get(filehandle_view.mount_id);
    auto mount_directory = mount_pair.first;
    if ( !mount_directory.valid()) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // invalid mount
      }
    auto mount_filehandle = mount_filehandle_t::view_binary(mount_pair.second);
    if (mount_filehandle.volume_file_id.VolumeSerialNumber != filehandle_view.volume_file_id.VolumeSerialNumber) {
        result.status = status_t::ERR_BADHANDLE;
        return result; // not the mount directly
      }

    auto file = mount_directory.by_id(filehandle_view.volume_file_id.FileId);
    if (!file.valid()) {
        result.status = status_t::ERR_ACCESS;
        return result;
      }

    FILE_BASIC_INFO basic_info;
    bool success = file.basic_info(basic_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }
    FILE_STANDARD_INFO standard_info;
    success = file.standard_info(standard_info);
    if (!success) {
        result.status = status_t::ERR_IO;
        return result;
      }

    result.file_wcc.before.set(wcc_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info));
    result.file_wcc.after.set(file_attr_from_BASIC_and_STANDARD_INFO(basic_info, standard_info, filehandle_view.volume_file_id));

    result.status = status_t::OK;
    result.verifier = this->cookie_verifier_m;
    DLOG(INFO) << "...success " << convert::to_string(file.fullpath());

    return result;
  }

  rpc_program_t rpc_program::describe()
  {
    using args_t = rpc_program_t::procedure_args_t;
    using result_t = rpc_program_t::procedure_result_t;

    rpc_program_t result;
    result.id = PROGRAM;
    result.version = VERSION;

    auto null_rpc = [=](const args_t& args)->result_t {
        DLOG(INFO) << "Got NULL in NFS";
        if (!args.parameter_reader.empty()) return {};
        nothing();
        return result_t::respond({});
      };
    auto get_attr_rpc = [=](const args_t& args)->result_t {
        auto reader = filehandle_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = get_attr(reader.read());
        return result_t::respond(write_get_attr_result(result));
      };
    auto set_attr_rpc = [=](const args_t& args)->result_t {
        auto reader = set_attr_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = set_attr(reader.read());
        return result_t::respond(write_set_attr_result(result));
      };
    auto lookup_rpc = [=](const args_t& args)->result_t {
        auto reader = dir_op_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = lookup(reader.read());
        return result_t::respond(write_lookup_result(result));
      };
    auto access_rpc = [=](const args_t& args)->result_t {
        auto reader = access_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = access(reader.read());
        return result_t::respond(write_access_result(result));
      };
    auto readlink_rpc = [=](const args_t& args)->result_t {
        auto reader = filehandle_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = readlink(reader.read());
        return result_t::respond(write_readlink_result(result));
      };
    auto read_rpc = [=](const args_t& args)->result_t {
        auto reader = read_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = read(reader.read());
        return result_t::respond(write_read_result(result));
      };
    auto write_rpc = [=](const args_t& args)->result_t {
        auto reader = write_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = write(reader.read());
        return result_t::respond(write_write_result(result));
      };
    auto create_rpc = [=](const args_t& args)->result_t {
        auto reader = create_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = create(reader.read());
        return result_t::respond(write_create_result(result));
      };
    auto mkdir_rpc = [=](const args_t& args)->result_t {
        auto reader = mkdir_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = mkdir(reader.read());
        return result_t::respond(write_create_result(result));
      };
    auto remove_rpc = [=](const args_t& args)->result_t {
        auto reader = dir_op_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = remove(reader.read());
        return result_t::respond(write_remove_result(result));
      };
    auto rmdir_rpc = [=](const args_t& args)->result_t {
        auto reader = dir_op_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = rmdir(reader.read());
        return result_t::respond(write_remove_result(result));
      };
    auto rename_rpc = [=](const args_t& args)->result_t {
        auto reader = rename_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = rename(reader.read());
        return result_t::respond(write_rename_result(result));
      };
    auto read_dir_rpc = [=](const args_t& args)->result_t {
        auto reader = read_dir_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = read_dir(reader.read());
        return result_t::respond(write_read_dir_result(result));
      };
    auto read_dir_plus_rpc = [=](const args_t& args)->result_t {
        auto reader = read_dir_plus_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = read_dir_plus(reader.read());
        return result_t::respond(write_read_dir_plus_result(result));
      };
    auto fs_stat_rpc = [=](const args_t& args)->result_t {
        auto reader = filehandle_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = fs_stat(reader.read());
        return result_t::respond(write_fs_stat_result(result));
      };
    auto fs_info_rpc = [=](const args_t& args)->result_t {
        auto reader = filehandle_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = fs_info(reader.read());
        return result_t::respond(write_fs_info_result(result));
      };
    auto path_conf_rpc = [=](const args_t& args)->result_t {
        auto reader = filehandle_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = path_conf(reader.read());
        return result_t::respond(write_path_conf_result(result));
      };
    auto commit_rpc = [=](const args_t& args)->result_t {
        auto reader = commit_args_reader_t(args.parameter_reader);
        if (!reader.valid()) return {};
        auto result = commit(reader.read());
        return result_t::respond(write_commit_result(result));
      };

    auto operation_not_supported = [](const args_t& args)->result_t {
        LOG(WARNING) << "Not supported NFS function has been called";
        binary_builder_t builder;
        builder.append32(nfs3::status_t::ERR_NOTSUPP);
        return result_t::respond(builder.build());
    };

    auto& calls = result.procedures;
    calls.set( 0, { "NULL", null_rpc });
    calls.set( 1, { "GETATTR", get_attr_rpc });
    calls.set( 2, { "SETATTR", set_attr_rpc });
    calls.set( 3, { "LOOKUP", lookup_rpc });
    calls.set( 4, { "ACCESS", access_rpc });
    calls.set( 5, { "READLINK", readlink_rpc });
    calls.set( 6, { "READ", read_rpc });
    calls.set( 7, { "WRITE", write_rpc });
    calls.set( 8, { "CREATE", create_rpc });
    calls.set( 9, { "MKDIR", mkdir_rpc });
    calls.set(10, { "SYMLINK", operation_not_supported/*symlink_rpc*/ });
    calls.set(11, { "MKNOD", operation_not_supported/*mk_node_rpc*/ });
    calls.set(12, { "REMOVE", remove_rpc });
    calls.set(13, { "RMDIR", rmdir_rpc });
    calls.set(14, { "RENAME", rename_rpc });
    calls.set(15, { "LINK", operation_not_supported/*link_rpc*/ });
    calls.set(16, { "READDIR", read_dir_rpc });
    calls.set(17, { "READDIRPLUS", read_dir_plus_rpc });
    calls.set(18, { "FSSTAT", fs_stat_rpc });
    calls.set(19, { "FSINFO", fs_info_rpc });
    calls.set(20, { "PATHCONF", path_conf_rpc });
    calls.set(21, { "COMMIT", commit_rpc });

    return result;
  }

} // namespace nfs3
