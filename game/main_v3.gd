extends "res://main_v2.gd"

# Scenario-aware wrapper around the V2 renderer. The C stream remains authoritative.
var active_scenario := "corner-market"
var scenario_difficulty := 1
var scenario_goal_type := 1
var scenario_goal_value := 0.0
var event_feed: Array[String] = []

func _ready() -> void:
    sprite_sheet = load("res://assets/shrink-city-sprites.png")
    security_sprite = load("res://assets/security-guard.png")
    var requested := "corner-market"
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--scenario="):
            requested = argument.trim_prefix("--scenario=")
    start_scenario(requested)

func start_scenario(slug: String) -> void:
    if sim_pipe != null:
        sim_pipe.close()
        sim_pipe = null
    active_scenario = slug
    pending = ""
    process_error = ""
    command_feedback = ""
    entities.clear()
    fixtures.clear()
    walls.clear()
    hazards.clear()
    rooms.clear()
    employees.clear()
    floor_rows.clear()
    metrics.clear()
    event_feed.clear()
    selected.clear()
    hovered.clear()
    last_tick = 0
    view_offset = Vector2(45.0, 70.0)
    zoom = 1.0

    var executable := ProjectSettings.globalize_path("res://../build/shrink-sim")
    var result := OS.execute_with_pipe(executable, ["--stream", "--realtime", "--ticks", "3600", "--scenario", slug, "--seed", "0"], false)
    if result.is_empty():
        process_error = "Build ../build/shrink-sim first"
    else:
        sim_pipe = result["stdio"]
    queue_redraw()

func _consume_line(line: String) -> void:
    var f := line.strip_edges().split(" ")
    if f.is_empty():
        return
    if f[0] == "SCENARIO" and f.size() >= 6:
        active_scenario = f[2]
        scenario_difficulty = int(f[3])
        scenario_goal_type = int(f[4])
        scenario_goal_value = float(f[5])
        return
    if f[0] == "EVENT" and f.size() >= 4:
        var message := _event_message(f[1], int(f[2]), int(f[3]))
        if message != "":
            event_feed.push_front(message)
            if event_feed.size() > 5:
                event_feed.resize(5)
        return
    super._consume_line(line)

func _event_message(kind: String, subject_id: int, amount: int) -> String:
    match kind:
        "CUSTOMER_ENTERED": return "Shopper #%d entered" % subject_id
        "PURCHASE_COMPLETED": return "Shopper #%d completed a purchase" % subject_id
        "THEFT_ATTEMPTED": return "Shoplifting attempt near fixture #%d" % amount
        "THEFT_DETECTED": return "Camera detected shopper #%d" % subject_id
        "SECURITY_RESPONDING": return "Security responding to shopper #%d" % subject_id
        "SECURITY_INTERVENTION": return "Security intervention at shopper #%d" % subject_id
        "THEFT_EXITED": return "LOSS PREVENTION: theft exited the store"
        "CHECKOUT_ABANDONED": return "Shopper #%d abandoned checkout" % subject_id
        "STOCKOUT": return "Merchandise stockout reported"
        _: return ""

func _scenario_floor_base() -> Color:
    match active_scenario:
        "electronics": return Color("#c7d1d8")
        "big-box": return Color("#cbc7bb")
        "pharmacy": return Color("#d8e5dc")
        "grocery-fresh": return Color("#dfd2ad")
        "troubled-store": return Color("#bdb9ae")
        _: return Color("#e1d2ad")

func _floor_color(x: int, y: int) -> Color:
    var base := _scenario_floor_base()
    if x >= 20:
        base = base.darkened(0.13)
    elif y <= 5:
        base = base.lightened(0.035)
    if active_scenario == "troubled-store" and (x * 7 + y * 11) % 13 == 0:
        return base.darkened(0.10)
    return base.lightened(0.025) if (x + y) % 2 == 0 else base.darkened(0.018)

func _draw_floor() -> void:
    # Preserve V2's strict tile geometry while giving each storefront a recognizable surface language.
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside(x, y):
                continue
            var c := _iso(Vector2(x, y))
            draw_colored_polygon(_diamond(c + Vector2(7, 8)), Color(0.02, 0.03, 0.04, 0.22))
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside(x, y):
                continue
            var c := _iso(Vector2(x, y))
            var color := _floor_color(x, y)
            draw_colored_polygon(_diamond(c), color)
            draw_polyline(PackedVector2Array([c + Vector2(0,-10), c + Vector2(20,0), c + Vector2(0,10), c + Vector2(-20,0), c + Vector2(0,-10)]), Color(0.20,0.22,0.20,0.11), 0.55)
            match active_scenario:
                "electronics":
                    if (x + y) % 3 == 0:
                        draw_line(c + Vector2(-10, 1), c + Vector2(10, 1), Color(0.36,0.48,0.56,0.15), 1.0)
                "pharmacy":
                    if x % 3 == 0 and y % 3 == 0:
                        draw_circle(c, 1.5, Color(0.55,0.70,0.63,0.22))
                "grocery-fresh":
                    draw_line(c + Vector2(-7, -2), c + Vector2(7, -2), Color(0.65,0.53,0.32,0.10), 0.8)
                "big-box":
                    if x % 5 == 0:
                        draw_line(c + Vector2(-15, 0), c + Vector2(15, 0), Color(0.52,0.49,0.42,0.10), 1.0)
                "troubled-store":
                    if (x * 5 + y * 3) % 17 == 0:
                        draw_line(c + Vector2(-5, -2), c + Vector2(6, 2), Color(0.30,0.29,0.27,0.18), 1.0)
                _:
                    if (x * 3 + y * 5) % 7 == 0:
                        draw_circle(c + Vector2(2, -1), 1.0, Color(0.54,0.43,0.28,0.16))

func _fixture_color(kind: String) -> Color:
    var base: Color = super._fixture_color(kind)
    if kind in ["entrance", "exit", "camera", "register", "self_checkout"]:
        return base
    match active_scenario:
        "electronics": return base.lerp(Color("#718da7"), 0.42)
        "pharmacy": return base.lerp(Color("#78a594"), 0.36)
        "grocery-fresh": return base.lerp(Color("#8fa65f"), 0.30)
        "big-box": return base.lerp(Color("#b27c4e"), 0.22)
        "troubled-store": return base.lerp(Color("#77736d"), 0.45)
        _: return base.lerp(Color("#c19a55"), 0.12)

func _storefront_identity() -> Dictionary:
    match active_scenario:
        "electronics": return {"name":"VOLT ELECTRONICS", "main":Color("#476f91"), "accent":Color("#9fd0e5")}
        "big-box": return {"name":"MEGAMART", "main":Color("#a45c3d"), "accent":Color("#f1c66b")}
        "pharmacy": return {"name":"GREEN CROSS PHARMACY", "main":Color("#4c8876"), "accent":Color("#b9dfcf")}
        "grocery-fresh": return {"name":"FRESH MARKET", "main":Color("#6c8f4d"), "accent":Color("#d5db87")}
        "troubled-store": return {"name":"VALUE TOWN", "main":Color("#7b6252"), "accent":Color("#d3b077")}
        _: return {"name":"SHRINK CITY MARKET", "main":Color("#9d8262"), "accent":Color("#e4c985")}

func _draw_frontage() -> void:
    var identity := _storefront_identity()
    var entrance := _iso(Vector2(1, 10))
    var main: Color = identity["main"]
    var accent: Color = identity["accent"]
    draw_line(entrance + Vector2(-31, 8), entrance + Vector2(31, 8), main, 8.0)
    draw_line(entrance + Vector2(-26, 2), entrance + Vector2(26, 2), accent, 3.0)
    draw_rect(Rect2(entrance + Vector2(-26, -31), Vector2(18, 28)), Color(0.55,0.78,0.84,0.38), true)
    draw_rect(Rect2(entrance + Vector2(8, -31), Vector2(18, 28)), Color(0.55,0.78,0.84,0.38), true)
    draw_string(ThemeDB.fallback_font, entrance + Vector2(-52, -44), str(identity["name"]), HORIZONTAL_ALIGNMENT_LEFT, -1, 13, accent.lightened(0.08))
    draw_circle(entrance + Vector2(-24, 12), 3.0, accent)
    draw_circle(entrance + Vector2(24, 12), 3.0, accent)

func _draw() -> void:
    super._draw()
    if event_feed.is_empty():
        return
    var y := 108.0
    draw_rect(Rect2(18, 96, 270, 24 + event_feed.size() * 20), Color(1.0, 1.0, 1.0, 0.88), true)
    draw_string(ThemeDB.fallback_font, Vector2(30, y), "LIVE STORE EVENTS", HORIZONTAL_ALIGNMENT_LEFT, -1, 12, Color("#315057"))
    y += 20.0
    for event in event_feed:
        draw_string(ThemeDB.fallback_font, Vector2(30, y), event, HORIZONTAL_ALIGNMENT_LEFT, 245, 11, Color("#26363a"))
        y += 19.0

func _exit_tree() -> void:
    if sim_pipe != null:
        sim_pipe.close()
        sim_pipe = null
