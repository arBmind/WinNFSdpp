import qbs

Product {
    type: "staticlibrary"

    name: "WinNFSdppLib"
    version: "0.0.0"
    files: [
        "binary/binary.h",
        "binary/binary_builder.cpp",
        "binary/binary_builder.h",
        "binary/binary_reader.cpp",
        "binary/binary_reader.h",
        "container/range_map.h",
        "container/string_convert.h",
        "meta/index_of.h",
        "meta/max.h",
        "meta/variant.h",
        "network/inet.cpp",
        "network/inet.h",
        "network/tcp.cpp",
        "network/tcp.h",
        "network/udp.cpp",
        "network/udp.h",
        "network/wsa_session.cpp",
        "network/wsa_session.h",
        "nfs/mount.cpp",
        "nfs/mount.h",
        "nfs/mount_aliases.cpp",
        "nfs/mount_aliases.h",
        "nfs/mount_cache.cpp",
        "nfs/mount_cache.h",
        "nfs/nfs3.cpp",
        "nfs/nfs3.h",
        "rpc/portmap.cpp",
        "rpc/portmap.h",
        "rpc/rpc.cpp",
        "rpc/rpc.h",
        "rpc/rpc_program.cpp",
        "rpc/rpc_program.h",
        "rpc/rpc_router.cpp",
        "rpc/rpc_router.h",
        "rpc/xdr.cpp",
        "rpc/xdr.h",
        "server/mount_server.cpp",
        "server/mount_server.h",
        "server/nfs3_server.cpp",
        "server/nfs3_server.h",
        "server/portmap_server.cpp",
        "server/portmap_server.h",
        "server/rpc_server.cpp",
        "server/rpc_server.h",
        "winfs/windows_handle.cpp",
        "winfs/windows_handle.h",
        "winfs/winfs.h",
        "winfs/winfs_directory.cpp",
        "winfs/winfs_directory.h",
        "winfs/winfs_file.cpp",
        "winfs/winfs_file.h",
        "winfs/winfs_object.cpp",
        "winfs/winfs_object.h",
        "wintime/wintime_convert.cpp",
        "wintime/wintime_convert.h",
    ]

    Depends { name: "cpp" }
    Depends { name: "GSL" }
    cpp.includePaths: [ "." ]
    cpp.dynamicLibraries: [ "ws2_32", "mswsock" ]
    cpp.minimumWindowsVersion: '6.2' // windows 8
    cpp.linkerFlags: "/ignore:4221"

    Export {
        Depends { name: "cpp" }
        Depends { name: "GSL" }
        cpp.cxxLanguageVersion: "c++14"
        cpp.includePaths: [ "." ]
        cpp.dynamicLibraries: [ "ws2_32", "mswsock" ]
        cpp.minimumWindowsVersion: '6.2' // windows 8
    }
}
