#include "experiments.h"

#include "winfs/winfs_object.h"

#include <gtest/gtest.h>

#include <string>

/*
 * Please note that in order to test this you basically need my disk setup.
 * If you know a better setup please let me know.
 * Otherwise ignore this test.
 */
#define BASEPATH L"D:/arB/Ansible/dresden-weekly/rails-example"
#define BASEPATH_WITH_SYMLINK L"C:/Users/Andreas/.vagrant.d/cache/ubuntu/trusty64"

TEST(object, path) {
  auto handle = winfs::open_path(BASEPATH);
  EXPECT_TRUE(handle.valid());
  auto path = handle.path();
  EXPECT_EQ(L"\\arB\\Ansible\\dresden-weekly\\rails-example", path);
}

TEST(object, path_with_symlink) {
  auto handle = winfs::open_path(BASEPATH_WITH_SYMLINK);
  EXPECT_TRUE(handle.valid());
  auto path = handle.path();
  EXPECT_EQ(L"\\vagrant-cache\\ubuntu\\trusty64", path);
}

TEST(object, fullpath) {
  auto handle = winfs::open_path(BASEPATH);
  EXPECT_TRUE(handle.valid());
  auto fullpath = handle.fullpath();
  EXPECT_EQ(L"\\\\?\\D:\\arB\\Ansible\\dresden-weekly\\rails-example", fullpath);
}

TEST(object, fullpath_cross_symlink) {
  auto path = L"C:/Users/Andreas/.vagrant.d/cache";
  auto handle = winfs::open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(path);
  EXPECT_TRUE(handle.valid());
  auto fullpath = handle.fullpath();
  EXPECT_EQ(L"", fullpath);
}

TEST(object, resolve_symlink_cross_symlink) {
  auto path = L"C:/Users/Andreas/.vagrant.d/cache";
  auto fullpath = winfs::resolve_symlink(path);
  EXPECT_EQ(L"Z:\\vagrant-cache", fullpath);
}

TEST(object, fullpath_cross_with_symlink) {
  auto handle = winfs::open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(BASEPATH_WITH_SYMLINK);
  EXPECT_TRUE(handle.valid());
  auto fullpath = handle.fullpath();
  EXPECT_EQ(L"", fullpath);
}

TEST(object, resolve_cross_with_symlink) {
  auto fullpath = winfs::deep_resolve_path(BASEPATH_WITH_SYMLINK);
  EXPECT_EQ(L"\\\\?\\Z:\\vagrant-cache\\ubuntu\\trusty64", fullpath);
}

TEST(object, fullpath_local_with_symlink) {
  auto path = L"D:\\arB\\Ansible\\hicknhack-private\\hnh-server\\ansible\\roles\\dresden-weekly.network-interfaces\\defaults";
  auto handle = winfs::open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(path);
  //auto handle = winfs::unique_object_t::by_path(path);
  EXPECT_TRUE(handle.valid());
  auto fullpath = handle.fullpath();
  EXPECT_EQ(L"\\\\?\\D:\\arB\\Ansible\\dresden-weekly\\network-interfaces\\defaults", fullpath);
}

TEST(object, fullpath_local_symlink) {
  auto path = L"D:\\arB\\Ansible\\hicknhack-private\\hnh-server\\ansible\\roles\\dresden-weekly.network-interfaces";
  auto handle = winfs::open_path<0, FILE_FLAG_BACKUP_SEMANTICS>(path);
  //auto handle = winfs::unique_object_t::by_path(path);
  EXPECT_TRUE(handle.valid());
  auto fullpath = handle.fullpath();
  EXPECT_EQ(L"\\\\?\\D:\\arB\\Ansible\\dresden-weekly\\network-interfaces", fullpath);
}
