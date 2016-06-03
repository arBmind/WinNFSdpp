import qbs

CppApplication {
    consoleApplication: true

    name: "WinfsTest"

    files: [
        "experiments.h",
        "winfs_test.cpp",
        "file_change_test.cpp",
    ]

    Depends { name: "WinNFSdppLib" }
    Depends { name: "GoogleTestMain" }
}
