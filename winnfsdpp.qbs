import qbs

Project {
    name: "WinNFSd++"
    //qbsSearchPaths: "qbs/"

    references: [
        "vendor",
        "src",
        "cli",
        "tests",
    ]

    Product {
        name: "Config"
        files: [ 'config.js', 'config.js.example' ]
    }
}
