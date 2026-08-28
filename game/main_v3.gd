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
        "PURCHASE": return "%d purchase%s completed" % [amount, "" if amount == 1 else "s"]
        "THEFT_RECORDED": return "LOSS PREVENTION: %d theft%s recorded" % [amount, "" if amount == 1 else "s"]
        "CUSTOMER_ABANDONED": return "%d shopper%s abandoned the trip" % [amount, "" if amount == 1 else "s"]
        _: return ""

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
