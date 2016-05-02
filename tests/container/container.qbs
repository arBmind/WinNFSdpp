import qbs

CppApplication {
    consoleApplication: true

    name: "ContainerTest"

    files: [
        "container_test.cpp",
    ]

    Depends { name: "WinNFSdppLib" }
    Depends { name: "GoogleTestMain" }
}
