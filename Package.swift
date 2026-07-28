// swift-tools-version: 6.0

import PackageDescription

let package = Package(
    name: "NoteMD",
    platforms: [
        .macOS(.v14)
    ],
    products: [
        .executable(name: "NoteMD", targets: ["NoteMD"])
    ],
    targets: [
        .executableTarget(
            name: "NoteMD",
            path: "Sources/Cadernos"
        )
    ]
)
