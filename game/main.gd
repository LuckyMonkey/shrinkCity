extends Node2D

# Presentation owns camera, selection, and build previews; C still owns simulation truth.
var sim_pipe: FileAccess
var sim_pid := -1
var pending := ""
var entities: Dictionary = {}
var metrics := {}
var last_tick := 0
var process_error := ""
var sim_seed := 12345
var sim_ticks := 600

var view_offset := Vector2(90.0, 82.0)
var zoom := 0.85
var dragging := false
var drag_distance := 0.0
var selected: Dictionary = {}
var build_mode := false
var build_kind := "shelf"
var build_cursor := Vector2(-1, -1)
var next_fixture_id := 100
var fixtures: Array[Dictionary] = []
var sprite_sheet: Texture2D
var hovered: Dictionary = {}
var mouse_position := Vector2.ZERO
var dragged_fixture_index := -1
var dragged_fixture_id := 0
var geometry_received := false
var geometry_width := 28
var geometry_height := 22
var guard_position := Vector2(16, 16)
var guard_time := 0.0
var guard_path := [Vector2(16, 16), Vector2(12, 16), Vector2(12, 9), Vector2(18, 9), Vector2(18, 16)]
var guard_segment := 0
var guard_progress := 0.0
var wall_mode := false
var wall_start := Vector2(-1, -1)
var wall_segments: Array[Dictionary] = []
var security_sprite: Texture2D

const ORIGIN := Vector2(430.0, 24.0)
const TILE_W := 40.0
const TILE_H := 20.0
const MAP_W := 28
const MAP_H := 22
const STATE_COLORS = {1: Color("#52b9ff"), 2: Color("#ffd166"), 3: Color("#73df91"), 4: Color("#ff7777")}
const PRODUCT_SYMBOLS = ["SNACK", "DRINK", "GUM", "MEAL"]
const BUILD_DEFS = {
    "shelf": {"label": "Gondola shelf", "symbol": "📦", "color": Color("#9d7049"), "cost": 80},
    "shelf_bin": {"label": "Open bin", "symbol": "🧺", "color": Color("#cf8b48"), "cost": 65},
    "short_shelf": {"label": "Short shelf", "symbol": "▤", "color": Color("#b08b62"), "cost": 55},
    "locked_shelf": {"label": "Locked shelf", "symbol": "🔒", "color": Color("#a66ba6"), "cost": 180},
    "clearance": {"label": "Clearance display", "symbol": "%", "color": Color("#db6a58"), "cost": 45},
    "camera": {"label": "Ceiling camera", "symbol": "📷", "color": Color("#7086a3"), "cost": 150},
    "register": {"label": "Staffed register", "symbol": "💵", "color": Color("#4f9d69"), "cost": 240},
    "entrance": {"label": "Entrance", "symbol": "IN", "color": Color("#65b7d6"), "cost": 500},
    "exit": {"label": "Exit", "symbol": "OUT", "color": Color("#65b7d6"), "cost": 500},
    "detector_gate": {"label": "Detector gate", "symbol": "║", "color": Color("#d7a94b"), "cost": 250},
    "rfid_station": {"label": "RFID tagging station", "symbol": "RF", "color": Color("#55a6c9"), "cost": 220},
    "self_checkout": {"label": "Self-checkout", "symbol": "🧾", "color": Color("#4eaa91"), "cost": 300},
    "locked_case": {"label": "Locked case", "symbol": "🔒", "color": Color("#b46a9e"), "cost": 220}
}

func _ready() -> void:
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--seed="): sim_seed = int(argument.trim_prefix("--seed="))
        elif argument.begins_with("--ticks="): sim_ticks = int(argument.trim_prefix("--ticks="))
    _make_fixtures()
    sprite_sheet = load("res://assets/shrink-city-sprites.png")
    security_sprite = load("res://assets/security-guard.png")
    var executable := ProjectSettings.globalize_path("res://../build/shrink-sim")
    var result := OS.execute_with_pipe(executable, ["--stream", "--realtime", "--ticks", str(sim_ticks), "--seed", str(sim_seed)], false)
    if result.is_empty(): process_error = "Could not start ../build/shrink-sim"
    else:
        sim_pipe = result["stdio"]
        sim_pid = int(result["pid"])
    queue_redraw()

func _exit_tree() -> void:
    # The stream is bounded and exits naturally; killing it during pipe teardown
    # triggers a double-free in some Godot/Linux combinations.
    sim_pipe = null

func _make_fixtures() -> void:
    # These are view-owned placeholders until fixture placement is moved into the C world API.
    for y in [4, 5, 6, 13, 14, 15]:
        _add_fixture("shelf", 5, y, "📦", "Merchandise shelf")
        _add_fixture("shelf", 8, y, "📦", "Merchandise shelf")
        _add_fixture("shelf", 12, y, "📦", "Merchandise shelf")
        _add_fixture("shelf", 15, y, "📦", "Merchandise shelf")
    _add_fixture("register", 18, 8, "💵", "Staffed register")
    _add_fixture("register", 18, 12, "💵", "Staffed register")
    _add_fixture("camera", 10, 2, "📷", "Camera coverage")
    _add_fixture("camera", 10, 19, "📷", "Camera coverage")
    _add_fixture("entrance", 1, 11, "🚪", "Entrance / exit")
    _add_fixture("employee", 17, 9, "🧑‍💼", "Employee")
    _add_fixture("security", 16, 16, "🕵️", "Loss prevention")
    _add_fixture("exit", 2, 6, "🚪", "Emergency exit")
    _add_fixture("entrance", 10, 1, "🚪", "Side entrance")
    _add_fixture("detector_gate", 3, 11, "║", "EAS detector gate")
    _add_fixture("rfid_station", 23, 5, "RF", "RFID tagging station")
    _add_fixture("shelf_bin", 10, 8, "🧺", "Produce bin")
    _add_fixture("short_shelf", 10, 12, "▤", "Clearance rack")
    _add_fixture("locked_shelf", 14, 8, "🔒", "High-value locked shelf")
    _add_fixture("clearance", 14, 12, "%", "Sale display")
    _make_walls()

func _add_fixture(kind: String, x: int, y: int, symbol: String, label: String) -> void:
    var department := "General merchandise"
    var unit_value := 8.0
    if kind == "shelf":
        var departments := ["Fresh foods", "Cold drinks", "Snacks", "Household"]
        department = departments[(x + y) % departments.size()]
        unit_value = [5.49, 3.99, 2.49, 7.99][(x + y) % 4]
    fixtures.append({"id": next_fixture_id, "kind": kind, "x": x, "y": y, "symbol": symbol, "label": label, "department": department, "unit_value": unit_value, "coverage": 8.0 if kind == "camera" else 0.0, "critical": kind == "entrance" or kind == "exit", "product_id": -1})
    next_fixture_id += 1

func _process(delta: float) -> void:
    guard_time += delta
    guard_progress += delta * 0.8
    if guard_progress >= 1.0:
        guard_progress = 0.0
        guard_segment = (guard_segment + 1) % guard_path.size()
    var next_guard := (guard_segment + 1) % guard_path.size()
    guard_position = guard_path[guard_segment].lerp(guard_path[next_guard], guard_progress)
    if sim_pipe != null:
        var available: int = sim_pipe.get_length()
        if available > 0:
            pending += sim_pipe.get_buffer(available).get_string_from_utf8()
            var lines: PackedStringArray = pending.split("\n")
            pending = lines[lines.size() - 1]
            lines.resize(lines.size() - 1)
            for line in lines: _consume_line(line)
    queue_redraw()

func _send_command(command: String) -> void:
    if sim_pipe != null:
        sim_pipe.store_line(command)
        sim_pipe.flush()

func _kind_from_type(type_id: int) -> String:
    var kinds := ["", "shelf", "shelf_bin", "short_shelf", "locked_shelf", "clearance", "register", "self_checkout", "camera", "entrance", "exit", "rfid_station", "locked_case"]
    return kinds[type_id] if type_id >= 0 and type_id < kinds.size() else "shelf"

func _fixture_from_type(id: int, type_id: int, x: int, y: int, rotation: int, product_id: int = -1) -> Dictionary:
    var kind := _kind_from_type(type_id)
    var definition: Dictionary = BUILD_DEFS.get(kind, {"label": "Fixture", "symbol": "#", "color": Color("#788898")})
    var label := str(definition.label)
    return {"id": id, "kind": kind, "x": x, "y": y, "rotation": rotation, "symbol": definition.symbol, "label": label, "department": "C-authoritative", "unit_value": 0.0, "coverage": 8.0 if kind == "camera" else 0.0, "critical": kind == "entrance" or kind == "exit", "product_id": product_id}

func _consume_line(line: String) -> void:
    var fields := line.strip_edges().split(" ")
    if fields.is_empty(): return
    if fields[0] == "GEOMETRY" and fields.size() >= 5:
        geometry_width = int(fields[1])
        geometry_height = int(fields[2])
        fixtures.clear()
        wall_segments.clear()
        geometry_received = true
    elif fields[0] == "WALL" and fields.size() >= 6:
        wall_segments.append({"id": int(fields[1]), "a": Vector2(int(fields[2]), int(fields[3])), "b": Vector2(int(fields[4]), int(fields[5]))})
    elif fields[0] == "FIXTURE" and fields.size() >= 6:
        var product_id := int(fields[6]) if fields.size() >= 7 else -1
        fixtures.append(_fixture_from_type(int(fields[1]), int(fields[2]), int(fields[3]), int(fields[4]), int(fields[5]), product_id))
    elif fields[0] == "COMMAND" and fields.size() >= 2:
        if int(fields[1]) != 0: process_error = "Construction rejected by C: status %s" % fields[1]
    elif fields[0] == "TICK" and fields.size() >= 8:
        last_tick = int(fields[1])
        metrics = {"revenue": float(fields[3]), "shrink": float(fields[4]), "labor": float(fields[5]), "wait": float(fields[6]), "satisfaction": float(fields[7])}
        entities.clear()
    elif fields[0] == "ENTITY" and fields.size() >= 6:
        entities[fields[1]] = {"id": fields[1], "state": int(fields[2]), "x": float(fields[3]), "y": float(fields[4]), "product": int(fields[5]), "target_fixture_id": int(fields[6]) if fields.size() >= 7 else 0, "target_x": int(fields[7]) if fields.size() >= 8 else 0, "target_y": int(fields[8]) if fields.size() >= 9 else 0}

func _iso(grid_position: Vector2) -> Vector2:
    return ORIGIN + Vector2((grid_position.x - grid_position.y) * TILE_W * 0.5, (grid_position.x + grid_position.y) * TILE_H * 0.5)

func _tile_polygon(center: Vector2) -> PackedVector2Array:
    return PackedVector2Array([center + Vector2(0, -TILE_H * 0.5), center + Vector2(TILE_W * 0.5, 0), center + Vector2(0, TILE_H * 0.5), center + Vector2(-TILE_W * 0.5, 0)])

func _screen_to_grid(screen_position: Vector2) -> Vector2:
    var local := (screen_position - view_offset) / zoom - ORIGIN
    return Vector2(floor(local.x / TILE_W + local.y / TILE_H + 0.5), floor(local.y / TILE_H - local.x / TILE_W + 0.5))

func _world_to_screen(grid_position: Vector2) -> Vector2:
    return view_offset + _iso(grid_position) * zoom

func _draw_fixture(fixture: Dictionary, alpha := 1.0) -> void:
    var center := _iso(Vector2(fixture.x, fixture.y))
    var definition: Dictionary = BUILD_DEFS.get(fixture.kind, {"color": Color("#788898")})
    var color: Color = definition.color
    color.a = alpha
    draw_colored_polygon(_tile_polygon(center), color.darkened(0.32))
    # Vector pictograms avoid platform-dependent emoji fonts and export cleanly.
    if fixture.kind == "shelf" or fixture.kind == "shelf_bin" or fixture.kind == "short_shelf" or fixture.kind == "locked_shelf" or fixture.kind == "clearance":
        var height := 28.0 if fixture.kind == "shelf" or fixture.kind == "locked_shelf" else 18.0 if fixture.kind == "short_shelf" else 12.0
        draw_rect(Rect2(center.x - 9, center.y - height, 18, height), color)
        draw_rect(Rect2(center.x - 12, center.y - 3, 24, 4), color.darkened(0.35))
        if fixture.kind == "shelf_bin" or fixture.kind == "clearance": draw_rect(Rect2(center.x - 12, center.y - 14, 24, 8), color.lightened(0.18))
        if fixture.kind == "locked_shelf": draw_line(Vector2(center.x, center.y - height + 3), Vector2(center.x, center.y - 4), Color("#e8d27b"), 2.0)
        for shelf_y in [center.y - 22, center.y - 14, center.y - 6]:
            draw_line(Vector2(center.x - 7, shelf_y), Vector2(center.x + 7, shelf_y), color.lightened(0.25), 2.0)
        var product_region: int = [8, 9, 8, 10][(int(fixture.x) + int(fixture.y)) % 4]
        _draw_sprite_region(product_region, center + Vector2(-4, -22), Vector2(14, 14), alpha)
        _draw_sprite_region(product_region, center + Vector2(4, -14), Vector2(12, 12), alpha)
    elif fixture.kind == "register" or fixture.kind == "self_checkout":
        draw_rect(Rect2(center.x - 12, center.y - 18, 24, 18), color.darkened(0.18))
        draw_rect(Rect2(center.x - 8, center.y - 27, 16, 11), color.lightened(0.14))
        draw_rect(Rect2(center.x - 5, center.y - 24, 10, 5), Color("#bce7dc"))
        draw_line(Vector2(center.x, center.y - 19), Vector2(center.x, center.y - 4), color.darkened(0.45), 2.0)
    elif fixture.kind == "camera":
        draw_circle(center + Vector2(0, -22), 92, Color(0.35, 0.65, 0.9, 0.10))
        draw_line(center + Vector2(0, -2), center + Vector2(0, -28), Color("#d9e6f2"), 3.0)
        draw_circle(center + Vector2(0, -34), 9, color)
        draw_circle(center + Vector2(3, -17), 3, Color("#d9f1ff"))
        draw_line(center + Vector2(-12, -24), center + Vector2(-4, -19), color, 4.0)
    elif fixture.kind == "detector_gate":
        draw_line(center + Vector2(-10, 0), center + Vector2(-10, -32), color, 4.0)
        draw_line(center + Vector2(10, 0), center + Vector2(10, -32), color, 4.0)
        draw_line(center + Vector2(-10, -28), center + Vector2(10, -28), color, 3.0)
    elif fixture.kind == "rfid_station":
        draw_rect(Rect2(center.x - 12, center.y - 22, 24, 18), color)
        draw_circle(center + Vector2(0, -13), 5, Color("#d8f4ff"))
    elif fixture.kind == "entrance" or fixture.kind == "exit":
        draw_line(center + Vector2(-10, 0), center + Vector2(-10, -28), color, 5.0)
        draw_line(center + Vector2(10, 0), center + Vector2(10, -28), color, 5.0)
        draw_arc(center + Vector2(0, -28), 10, PI, TAU, 12, color, 5.0)
    elif fixture.kind == "employee":
        _draw_sprite_region(6, center, Vector2(62, 76), alpha)
    elif fixture.kind == "security":
        if security_sprite != null:
            draw_texture_rect(security_sprite, Rect2(center + Vector2(-28, -84), Vector2(56, 84)), false, Color(1, 1, 1, alpha))
    elif fixture.kind == "locked_case":
        draw_rect(Rect2(center.x - 13, center.y - 24, 26, 22), color.darkened(0.1))
        draw_rect(Rect2(center.x - 9, center.y - 20, 18, 14), Color(0.7, 0.9, 1.0, 0.32))
        draw_circle(center + Vector2(0, -13), 4, Color("#f8d66d"))
        draw_line(center + Vector2(0, -13), center + Vector2(0, -7), Color("#f8d66d"), 2.0)
    else:
        draw_rect(Rect2(center.x - 10, center.y - 16, 20, 16), color)
    if fixture.has("label"):
        draw_string(ThemeDB.fallback_font, center + Vector2(-18, 13), str(fixture.label), HORIZONTAL_ALIGNMENT_LEFT, 72, 9, Color("#4c4036"))

func _draw_sprite_region(region_index: int, center: Vector2, size: Vector2, alpha: float) -> void:
    if sprite_sheet == null: return
    var cell := Vector2(float(sprite_sheet.get_width()) / 4.0, float(sprite_sheet.get_height()) / 3.0)
    var source := Rect2(Vector2(region_index % 4, region_index / 4) * cell, cell)
    draw_texture_rect_region(sprite_sheet, Rect2(center - Vector2(size.x * 0.5, size.y), size), source, Color(1, 1, 1, alpha))

func _draw_roaming_guard() -> void:
    var center := _iso(guard_position)
    draw_circle(center + Vector2(0, -7), 15.0, Color(0.08, 0.12, 0.16, 0.42))
    draw_circle(center + Vector2(0, -35), 105.0, Color(0.92, 0.68, 0.25, 0.10))
    if security_sprite != null:
        draw_texture_rect(security_sprite, Rect2(center + Vector2(-30, -90), Vector2(60, 90)), false, Color.WHITE)

func _inside_store(x: int, y: int) -> bool:
    if x < 1 or y < 1 or x >= 27 or y >= 21: return false
    if x <= 2 and y <= 3: return false
    if x <= 1 and y >= 18: return false
    if x >= 25 and y <= 2: return false
    if x >= 25 and y >= 19: return false
    return true

func _make_walls() -> void:
    wall_segments = [
        {"a": Vector2(2, 4), "b": Vector2(2, 1)}, {"a": Vector2(2, 1), "b": Vector2(18, 1)},
        {"a": Vector2(18, 1), "b": Vector2(20, 3)}, {"a": Vector2(20, 3), "b": Vector2(26, 3)},
        {"a": Vector2(26, 3), "b": Vector2(26, 18)}, {"a": Vector2(26, 18), "b": Vector2(24, 20)},
        {"a": Vector2(24, 20), "b": Vector2(3, 20)}, {"a": Vector2(3, 20), "b": Vector2(1, 17)},
        {"a": Vector2(1, 17), "b": Vector2(1, 13)}, {"a": Vector2(1, 9), "b": Vector2(1, 6)},
        {"a": Vector2(1, 6), "b": Vector2(2, 4)}, {"a": Vector2(20, 3), "b": Vector2(20, 20)},
        {"a": Vector2(20, 11), "b": Vector2(26, 11)}
    ]

func _draw_room_label(label: String, grid_position: Vector2) -> void:
    draw_string(ThemeDB.fallback_font, _iso(grid_position), label, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, Color("#635443"))

func _draw() -> void:
    draw_rect(Rect2(0, 0, 1280, 720), Color("#101820"))
    draw_set_transform(view_offset, 0.0, Vector2(zoom, zoom))
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside_store(x, y): continue
            var center := _iso(Vector2(x, y))
            var floor_color := Color("#d9c79e") if (x + y) % 2 == 0 else Color("#d1bc91")
            if x >= 20 and y <= 10: floor_color = Color("#c5b9a3")
            elif x >= 20: floor_color = Color("#b9b0a5")
            draw_colored_polygon(_tile_polygon(center), floor_color)
            draw_polyline(PackedVector2Array([center + Vector2(0, -TILE_H * 0.5), center + Vector2(TILE_W * 0.5, 0), center + Vector2(0, TILE_H * 0.5), center + Vector2(-TILE_W * 0.5, 0), center + Vector2(0, -TILE_H * 0.5)]), Color(0.35, 0.29, 0.22, 0.22), 0.7)
    var wall_color := Color("#463b34")
    for wall in wall_segments:
        draw_line(_iso(wall.a), _iso(wall.b), wall_color, 7.0)
    if wall_mode and wall_start.x >= 0:
        draw_line(_iso(wall_start), _iso(build_cursor), Color("#f1c75b"), 5.0)
    _draw_room_label("SALES FLOOR", Vector2(7, 2))
    _draw_room_label("STOCKROOM", Vector2(22, 3))
    _draw_room_label("STAFF / SECURITY", Vector2(21, 14))
    for fixture in fixtures:
        if fixture.kind != "security": _draw_fixture(fixture)
    _draw_roaming_guard()
    if build_mode and build_cursor.x >= 0 and build_cursor.x < MAP_W and build_cursor.y >= 0 and build_cursor.y < MAP_H:
        var definition: Dictionary = BUILD_DEFS[build_kind]
        _draw_fixture({"x": build_cursor.x, "y": build_cursor.y, "kind": build_kind, "symbol": definition.symbol}, 0.55)
    for data in entities.values():
        var center := _iso(Vector2(data.x, data.y))
        var state: int = data.state
        var color: Color = STATE_COLORS.get(state, Color.WHITE)
        var variant := int(data.id) % 6
        _draw_sprite_region(variant, center, Vector2(70, 82), 1.0)
        var product_region: int = [8, 9, 8, 10][data.product]
        _draw_sprite_region(product_region, center + Vector2(24, -46), Vector2(25, 25), 1.0)
        draw_circle(center + Vector2(-25, -67), 4.0, color)
    draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
    _draw_ui()

func _draw_ui() -> void:
    draw_string(ThemeDB.fallback_font, Vector2(28, 36), "SHRINK CITY  •  STORE BUILDER", HORIZONTAL_ALIGNMENT_LEFT, -1, 25, Color.WHITE)
    draw_string(ThemeDB.fallback_font, Vector2(30, 61), "Drag pan  •  wheel zoom  •  click inspect  •  B build  •  W draw walls  •  1-9 choose  •  right-click remove  •  R reset", HORIZONTAL_ALIGNMENT_LEFT, -1, 14, Color("#a9bdc9"))
    draw_rect(Rect2(980, 82, 275, 590), Color("#243442"))
    draw_string(ThemeDB.fallback_font, Vector2(1002, 116), "STORE CONTROL", HORIZONTAL_ALIGNMENT_LEFT, -1, 20, Color.WHITE)
    var state_text := "BUILD MODE: %s\nAddon: %s\nCost: $%d" % ["ON" if build_mode else "OFF", BUILD_DEFS[build_kind].label, BUILD_DEFS[build_kind].cost]
    draw_multiline_string(ThemeDB.fallback_font, Vector2(1002, 150), state_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, 24, Color("#ffd166") if build_mode else Color("#d8e5ef"))
    var metric_text := "\nTICK %d\nCustomers: %d\n\nRevenue   $%.2f\nShrink    $%.2f\nLabor     $%.2f\nWait      %.1f sec\nSatisfaction %.1f%%" % [last_tick, entities.size(), metrics.get("revenue", 0.0), metrics.get("shrink", 0.0), metrics.get("labor", 0.0), metrics.get("wait", 0.0), metrics.get("satisfaction", 0.0)]
    draw_multiline_string(ThemeDB.fallback_font, Vector2(1002, 235), metric_text, HORIZONTAL_ALIGNMENT_LEFT, -1, 16, 25, Color("#d8e5ef"))
    draw_string(ThemeDB.fallback_font, Vector2(1002, 430), "INSPECTOR", HORIZONTAL_ALIGNMENT_LEFT, -1, 18, Color.WHITE)
    if selected.is_empty():
        draw_multiline_string(ThemeDB.fallback_font, Vector2(1002, 462), "Click a customer or\nfixture to inspect it.", HORIZONTAL_ALIGNMENT_LEFT, -1, 15, 23, Color("#9fb3c0"))
    else:
        var inspector := "SELECTED\n%s\nID: %s\nPosition: (%s, %s)\nDepartment: %s\nValue: $%.2f\nCoverage: %.1f" % [selected.get("label", "Customer"), selected.get("id", "—"), selected.get("x", "—"), selected.get("y", "—"), selected.get("department", PRODUCT_SYMBOLS[selected.get("product", 0)] if selected.has("product") else "—"), selected.get("unit_value", 0.0), selected.get("coverage", 0.0)]
        draw_multiline_string(ThemeDB.fallback_font, Vector2(1002, 462), inspector, HORIZONTAL_ALIGNMENT_LEFT, -1, 15, 23, Color("#f1dfb4"))
    if not hovered.is_empty() and mouse_position.x < 970.0:
        var tip := "%s\n%s\nValue: $%.2f" % [hovered.get("label", "Customer"), hovered.get("department", "Live simulation"), hovered.get("unit_value", 0.0)]
        var tip_position := Vector2(clamp(mouse_position.x + 16.0, 8.0, 900.0), clamp(mouse_position.y + 16.0, 80.0, 650.0))
        draw_rect(Rect2(tip_position, Vector2(190, 58)), Color("#18242d"))
        draw_string(ThemeDB.fallback_font, tip_position + Vector2(10, 20), tip, HORIZONTAL_ALIGNMENT_LEFT, 170, 13, Color("#f4e1ad"))
    if process_error != "": draw_string(ThemeDB.fallback_font, Vector2(1002, 640), process_error, HORIZONTAL_ALIGNMENT_LEFT, 240, 13, Color("#ff7777"))

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_LEFT:
            if event.pressed:
                dragged_fixture_index = _fixture_index_at(event.position)
                drag_distance = 0.0
                if dragged_fixture_index >= 0:
                    selected = fixtures[dragged_fixture_index]
                    dragged_fixture_id = int(fixtures[dragged_fixture_index].id)
                    dragging = false
                else:
                    dragging = true
            else:
                dragging = false
                if dragged_fixture_index >= 0:
                    var moved: Dictionary = fixtures[dragged_fixture_index]
                    _send_command("MOVE %d %d %d" % [int(dragged_fixture_id), int(moved.x), int(moved.y)])
                    dragged_fixture_index = -1
                    dragged_fixture_id = 0
                elif drag_distance < 8.0:
                    _click_world(event.position)
        elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
            zoom = clamp(zoom * 1.1, 0.55, 1.8)
        elif event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            zoom = clamp(zoom / 1.1, 0.55, 1.8)
        elif event.pressed and event.button_index == MOUSE_BUTTON_RIGHT:
            _remove_fixture_at(event.position)
    elif event is InputEventMouseMotion:
        mouse_position = event.position
        hovered = _hover_at(event.position)
        if dragged_fixture_index >= 0:
            var tile := _screen_to_grid(event.position)
            if _inside_store(int(tile.x), int(tile.y)):
                fixtures[dragged_fixture_index].x = int(tile.x)
                fixtures[dragged_fixture_index].y = int(tile.y)
                selected = fixtures[dragged_fixture_index]
            drag_distance += event.relative.length()
        elif dragging:
            view_offset += event.relative
            drag_distance += event.relative.length()
        build_cursor = _screen_to_grid(event.position)
    elif event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_B: build_mode = not build_mode
        elif event.keycode == KEY_W: wall_mode = not wall_mode
        elif event.keycode == KEY_1: build_kind = "shelf"
        elif event.keycode == KEY_2: build_kind = "shelf_bin"
        elif event.keycode == KEY_3: build_kind = "short_shelf"
        elif event.keycode == KEY_4: build_kind = "locked_shelf"
        elif event.keycode == KEY_5: build_kind = "clearance"
        elif event.keycode == KEY_6: build_kind = "camera"
        elif event.keycode == KEY_7: build_kind = "detector_gate"
        elif event.keycode == KEY_8: build_kind = "rfid_station"
        elif event.keycode == KEY_9: build_kind = "self_checkout"
        elif event.keycode == KEY_R:
            view_offset = Vector2(90.0, 82.0)
            zoom = 0.85
            wall_start = Vector2(-1, -1)
        queue_redraw()

func _fixture_index_at(screen_position: Vector2) -> int:
    var closest_distance := 30.0
    var closest := -1
    for i in range(fixtures.size()):
        var fixture: Dictionary = fixtures[i]
        var distance := _world_to_screen(Vector2(fixture.x, fixture.y)).distance_to(screen_position)
        if distance < closest_distance:
            closest_distance = distance
            closest = i
    return closest

func _hover_at(screen_position: Vector2) -> Dictionary:
    var index := _fixture_index_at(screen_position)
    if index >= 0: return fixtures[index]
    for data in entities.values():
        if _world_to_screen(Vector2(data.x, data.y)).distance_to(screen_position) < 32.0:
            return {"label": "Customer", "id": data.id, "x": snapped(data.x, 0.1), "y": snapped(data.y, 0.1), "department": PRODUCT_SYMBOLS[data.product], "unit_value": 0.0, "coverage": 0.0}
    return {}

func _click_world(screen_position: Vector2) -> void:
    if screen_position.x > 980.0:
        return
    var tile := _screen_to_grid(screen_position)
    if wall_mode:
        if wall_start.x < 0: wall_start = tile
        elif _inside_store(int(tile.x), int(tile.y)):
            _send_command("WALL %d %d %d %d" % [int(wall_start.x), int(wall_start.y), int(tile.x), int(tile.y)])
            wall_start = Vector2(-1, -1)
        return
    if build_mode:
        if _inside_store(int(tile.x), int(tile.y)) and not _fixture_at(tile):
            var definition: Dictionary = BUILD_DEFS[build_kind]
            _send_command("PLACE %s %d %d 0" % [build_kind, int(tile.x), int(tile.y)])
        return
    var closest_distance := 26.0
    selected = {}
    var fixture_index := _fixture_index_at(screen_position)
    if fixture_index >= 0:
        selected = fixtures[fixture_index]
        closest_distance = 30.0
    for data in entities.values():
        var entity_distance := _world_to_screen(Vector2(data.x, data.y)).distance_to(screen_position)
        if entity_distance < closest_distance:
            closest_distance = entity_distance
            selected = {"label": "Customer", "id": data.get("id", "live"), "x": snapped(data.x, 0.1), "y": snapped(data.y, 0.1), "state": data.state, "product": data.product}
    queue_redraw()

func _remove_fixture_at(screen_position: Vector2) -> void:
    var tile := _screen_to_grid(screen_position)
    for i in range(fixtures.size() - 1, -1, -1):
        var fixture: Dictionary = fixtures[i]
        if fixture.x == int(tile.x) and fixture.y == int(tile.y) and (int(fixture.id) >= 100 or not fixture.critical) and _critical_routes_safe(fixture):
            _send_command("REMOVE %d" % int(fixture.id))
            selected = {}
            return

func _critical_routes_safe(_candidate: Dictionary) -> bool:
    # C is the only authority for door/connectivity validation.
    return true

func _fixture_at(tile: Vector2) -> bool:
    for fixture in fixtures:
        if fixture.x == int(tile.x) and fixture.y == int(tile.y): return true
    return false
