import qbs

CppApplication {
    consoleApplication: true

    name: "WinNFSdpp"
    version: "0.0.0"

    files: [
        "main.cpp",
    ]

    Depends { name: "cpp" }
    Depends { name: "WinNFSdppLib" }
}
