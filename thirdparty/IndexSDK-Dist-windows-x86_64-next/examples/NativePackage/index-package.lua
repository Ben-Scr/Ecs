return {
    name = "Example",
    version = "0.1.0",
    description = "Minimal native package built against the exact IndexSDK engine binary.",
    layers = {
        native = {
            sources = { "src/**.cpp", "src/**.hpp", "include/**.hpp" },
            includes = { "include" },
        },
    },
    dependencies = {},
}