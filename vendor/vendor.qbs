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
}
