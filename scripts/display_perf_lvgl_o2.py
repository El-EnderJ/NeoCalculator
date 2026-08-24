"""Apply -O2 only to LVGL C/C++ translation units on production WROOM builds."""

Import("env")


def optimize_lvgl(build_env, node):
    source_path = node.srcnode().get_abspath().replace("\\", "/")
    if "/lvgl/" not in source_path:
        return node

    # WHY: BOARD A telemetry shows launcher composition, not SPI transfer,
    # dominates the frame budget. Keep the application/CAS at the established
    # -Os and optimize only LVGL's own translation units.
    optimized_env = build_env.Clone()
    optimized_env.ProcessUnFlags("-Os")
    optimized_env.AppendUnique(CCFLAGS=["-O2"])
    return optimized_env.Object(node)


env.AddBuildMiddleware(optimize_lvgl)
