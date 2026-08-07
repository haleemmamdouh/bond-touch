// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "BondTouchiOS",
    platforms: [.iOS(.v16)],
    dependencies: [
        .package(url: "https://github.com/emqx/CocoaMQTT.git", from: "2.1.0")
    ],
    targets: [
        .target(
            name: "BondTouchiOS",
            dependencies: ["CocoaMQTT"],
            path: "BondTouchiOS"
        )
    ]
)
