import qbs

CppApplication {
    id: root
    consoleApplication: true

    name: "WinNFSdpp"
    version: "0.0.0"

    files: [
        "cli.h",
        "main.cpp",
    ]
    Depends { name: "cpp" }
    Depends { name: "WinNFSdppLib" }
    Depends { name: "GLog" }

    cpp.defines: "WINNFSDPP_VERSION=\"" + root.version + "\""
}
