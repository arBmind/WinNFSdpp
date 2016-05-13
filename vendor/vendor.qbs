import qbs
import qbs.FileInfo
import '../config.js' as Config

Project {
    name: "Vendor"

    Product {
        name: "GSL"

        files: [
            "GSL/gsl.h",
            "GSL/gsl_assert.h",
            "GSL/gsl_util.h",
            "GSL/span.h",
            "GSL/string_span.h",
        ]

        Export {
            Depends { name: "cpp" }
            cpp.runtimeLibrary: "static"
            cpp.cxxLanguageVersion: "c++14"
            cpp.includePaths: [ "." ]
            cpp.enableExceptions: true
        }
    }

    Product {
        name: "GoogleTest"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: Config.googleTestIncludePath()
            cpp.libraryPaths: Config.googleTestLibPath(qbs)
            cpp.staticLibraries: Config.googleTestLib
        }
    }

    Product {
        name: "GoogleTestMain"

        Depends { name: "GoogleTest" }

        Export {
            Depends { name: "GoogleTest" }
            Depends { name: "cpp" }
            cpp.staticLibraries: Config.googleTestMainLib
        }
    }

    Product {
        name: "GFlags"

        Export {
            Depends { name: "cpp" }
            cpp.includePaths: Config.gFlagsIncludePath()
            cpp.libraryPaths: Config.gFlagsLibPath(qbs)
            cpp.staticLibraries: Config.gFlagsLib
        }
    }

    Product {
        name: "GLog"

        Depends { name: "GFlags" }

        Export {
            Depends { name: "cpp"}
            Depends { name: "GFlags"}
            cpp.includePaths: Config.gLogIncludePath()
            cpp.libraryPaths: Config.gLogLibPath(qbs)
            cpp.staticLibraries: Config.gLogLib
        }
    }
}
