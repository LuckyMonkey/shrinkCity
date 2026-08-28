extends Node2D

# Visual overhaul prototype. C remains authoritative for geometry and simulation.
var sim_pipe: FileAccess
var pending := ""
var process_error := ""
var command_feedback := ""
var feedback_time := 0.0
var entities: Dictionary = {}
var fixtures: Array[Dictionary] = []
var walls: Array[Dictionary] = []
var rooms: Array[Dictionary] = []
var floor_rows: Array[int] = []
var metrics := {}
var employees: Array[Dictionary] = []
var geometry_width := 28
var geometry_height := 22
var layout_id := 0
var last_tick := 0
var selected: Dictionary = {}
var hovered: Dictionary = {}
var mouse_position := Vector2.ZERO
var dragging := false
var view_offset := Vector2(45.0, 70.0)
var zoom := 1.0
var build_mode := false
var build_kind := "shelf"
var build_cursor := Vector2(-1, -1)
var build_rotation := 0
var dragged_id := 0
var dragged_fixture := false
var debug_overlay := false
var sprite_sheet: Texture2D
var security_sprite: Texture2D

const ORIGIN := Vector2(430.0, 24.0)
const TILE_W := 40.0
const TILE_H := 20.0
const WORLD_PANEL_RIGHT := 1010.0
const PRODUCT_SYMBOLS := ["SNACK", "DRINK", "GUM", "MEAL"]
const BUILD_ORDER := ["shelf", "bin", "short_shelf", "locked_shelf", "clearance", "register", "self_checkout", "camera"]
const BUILD_DEFS := {
    "shelf": {"name":"Gondola", "type":"shelf", "cost":80, "color":Color("#8b6547")},
    "bin": {"name":"Dump bin", "type":"shelf_bin", "cost":65, "color":Color("#bb7547")},
    "short_shelf": {"name":"Low shelf", "type":"short_shelf", "cost":55, "color":Color("#9f8062")},
    "locked_shelf": {"name":"Locked case", "type":"locked_shelf", "cost":180, "color":Color("#84628b")},
    "clearance": {"name":"Promo table", "type":"clearance", "cost":45, "color":Color("#c86d55")},
    "register": {"name":"Register", "type":"register", "cost":240, "color":Color("#4e8c63")},
    "self_checkout": {"name":"Self checkout", "type":"self_checkout", "cost":300, "color":Color("#468d7d")},
    "camera": {"name":"Camera", "type":"camera", "cost":150, "color":Color("#667d95")}
}

func _ready() -> void:
    sprite_sheet = load("res://assets/shrink-city-sprites.png")
    security_sprite = load("res://assets/security-guard.png")
    var seed := 12345
    var ticks := 600
    for argument in OS.get_cmdline_user_args():
        if argument.begins_with("--seed="): seed = int(argument.trim_prefix("--seed="))
        elif argument.begins_with("--ticks="): ticks = int(argument.trim_prefix("--ticks="))
    var executable := ProjectSettings.globalize_path("res://../build/shrink-sim")
    var result := OS.execute_with_pipe(executable, ["--stream", "--realtime", "--ticks", str(ticks), "--seed", str(seed)], false)
    if result.is_empty():
        process_error = "Build ../build/shrink-sim first"
    else:
        sim_pipe = result["stdio"]
    queue_redraw()

func _process(delta: float) -> void:
    if feedback_time > 0.0: feedback_time -= delta
    if sim_pipe != null:
        var available := sim_pipe.get_length()
        if available > 0:
            pending += sim_pipe.get_buffer(available).get_string_from_utf8()
            var lines: PackedStringArray = pending.split("\n")
            pending = lines[lines.size() - 1]
            lines.resize(lines.size() - 1)
            for line in lines: _consume_line(line)
    queue_redraw()

func _consume_line(line: String) -> void:
    var f := line.strip_edges().split(" ")
    if f.is_empty(): return
    match f[0]:
        "GEOMETRY":
            if f.size() >= 3:
                geometry_width = int(f[1]); geometry_height = int(f[2])
                layout_id = int(f[5]) if f.size() >= 6 else 0
                floor_rows.clear(); fixtures.clear(); rooms.clear(); walls.clear(); employees.clear()
                for _y in range(geometry_height): floor_rows.append(0)
        "FLOOR":
            if f.size() >= 3:
                var y := int(f[1])
                if y >= 0 and y < floor_rows.size(): floor_rows[y] = int(f[2])
        "ROOM":
            if f.size() >= 9:
                rooms.append({"id":int(f[1]), "type":int(f[2]), "x":int(f[3]), "y":int(f[4]), "width":int(f[5]), "height":int(f[6]), "customer":int(f[7]), "staff":int(f[8])})
        "WALL":
            if f.size() >= 6:
                walls.append({"id":int(f[1]), "a":Vector2(int(f[2]), int(f[3])), "b":Vector2(int(f[4]), int(f[5]))})
        "FIXTURE":
            if f.size() >= 6:
                fixtures.append({"id":int(f[1]), "kind":_kind_from_type(int(f[2])), "x":int(f[3]), "y":int(f[4]), "rotation":int(f[5]), "width":int(f[6]) if f.size() >= 7 else 1, "height":int(f[7]) if f.size() >= 8 else 1, "access":int(f[8]) if f.size() >= 9 else 0, "product":int(f[9]) if f.size() >= 10 else -1})
        "TICK":
            if f.size() >= 8:
                last_tick = int(f[1])
                metrics = {"revenue":float(f[3]), "shrink":float(f[4]), "labor":float(f[5]), "wait":float(f[6]), "satisfaction":float(f[7])}
                entities.clear()
        "ENTITY":
            if f.size() >= 6:
                entities[f[1]] = {"id":int(f[1]), "state":int(f[2]), "x":float(f[3]), "y":float(f[4]), "product":int(f[5]), "target_fixture_id":int(f[6]) if f.size() >= 7 else 0, "target_x":int(f[7]) if f.size() >= 8 else 0, "target_y":int(f[8]) if f.size() >= 9 else 0, "archetype":int(f[9]) if f.size() >= 10 else 0, "speed":float(f[10]) if f.size() >= 11 else 0.0, "patience":float(f[11]) if f.size() >= 12 else 0.0, "budget":float(f[12]) if f.size() >= 13 else 0.0, "theft_tendency":float(f[13]) if f.size() >= 14 else 0.0}
        "EMPLOYEE":
            if f.size() >= 9: employees.append({"id":int(f[1]), "role":int(f[2]), "wage":float(f[3]), "skill":float(f[4]), "fatigue":float(f[5]), "morale":float(f[6]), "x":int(f[7]), "y":int(f[8])})
        "COMMAND":
            if f.size() >= 2:
                var status := int(f[1])
                var labels := {0:"Construction accepted", 1:"Outside store bounds", 2:"Overlaps another fixture", 3:"Would block a required route", 4:"Invalid entrance / exit", 5:"Invalid wall", 6:"Entity not found"}
                command_feedback = labels.get(status, "Construction rejected (%d)" % status)
                feedback_time = 3.0
                if status != 0: process_error = command_feedback

func _kind_from_type(type_id: int) -> String:
    var kinds := ["", "shelf", "shelf_bin", "short_shelf", "locked_shelf", "clearance", "register", "self_checkout", "camera", "entrance", "exit", "rfid_station", "locked_case"]
    return kinds[type_id] if type_id > 0 and type_id < kinds.size() else "fixture"

func _iso(p: Vector2) -> Vector2:
    return ORIGIN + Vector2((p.x - p.y) * TILE_W * 0.5, (p.x + p.y) * TILE_H * 0.5)

func _screen(p: Vector2) -> Vector2:
    return view_offset + _iso(p) * zoom

func _grid(screen_pos: Vector2) -> Vector2:
    var p := (screen_pos - view_offset) / zoom - ORIGIN
    return Vector2(floor(p.x / TILE_W + p.y / TILE_H + 0.5), floor(p.y / TILE_H - p.x / TILE_W + 0.5))

func _diamond(c: Vector2, inset := 0.0) -> PackedVector2Array:
    return PackedVector2Array([c+Vector2(0,-TILE_H*0.5+inset), c+Vector2(TILE_W*0.5-inset,0), c+Vector2(0,TILE_H*0.5-inset), c+Vector2(-TILE_W*0.5+inset,0)])

func _inside(x: int, y: int) -> bool:
    if x < 0 or y < 0 or x >= geometry_width or y >= geometry_height: return false
    if y >= floor_rows.size(): return false
    return (floor_rows[y] & (1 << x)) != 0

func _zone_for(x: int, y: int) -> int:
    # Presentation-only visual zoning until authoritative departments land in snapshots.
    if x >= 20 and y <= 10: return 3
    if x >= 20: return 4
    if y <= 7: return 0
    if y >= 15: return 2
    return 1

func _floor_color(x: int, y: int) -> Color:
    var zone := _zone_for(x,y)
    var palette := [Color("#d9c39a"), Color("#d7c8a7"), Color("#c9bfaa"), Color("#b9b3a5"), Color("#afa99e")]
    var c: Color = palette[zone]
    return c.lightened(0.035) if (x + y) % 2 == 0 else c.darkened(0.025)

func _draw() -> void:
    draw_rect(Rect2(0,0,1280,720), Color("#dce8df"))
    draw_set_transform(view_offset, 0.0, Vector2(zoom,zoom))
    _draw_site()
    _draw_floor()
    _draw_rooms()
    _draw_building_edges()
    _draw_walls()
    _draw_frontage()
    _draw_fixtures()
    _draw_people()
    _draw_employees()
    _draw_build_preview()
    if debug_overlay: _draw_debug_overlay()
    draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
    _draw_hud()

func _draw_site() -> void:
    # A compact site apron makes the store feel like a property, not a floating debug diamond.
    for y in range(-2, geometry_height + 7):
        for x in range(-4, geometry_width + 5):
            if _inside(x,y): continue
            var c := _iso(Vector2(x,y))
            var color := Color("#88aa7d")
            if y >= geometry_height + 1: color = Color("#7a8584")
            elif x <= 1 and y >= 8 and y <= 16: color = Color("#8c8779")
            elif (x * 7 + y * 11) % 8 == 0: color = Color("#719968")
            draw_colored_polygon(_diamond(c), color)
    for x in range(1, 20, 3):
        var p := _iso(Vector2(x, geometry_height + 3))
        draw_line(p+Vector2(-13,0), p+Vector2(13,0), Color("#d9cf94"), 1.5)
    var road_a := _iso(Vector2(-2, geometry_height + 5))
    var road_b := _iso(Vector2(geometry_width + 3, geometry_height + 5))
    draw_line(road_a, road_b, Color("#777e81"), 2.0)
    for tree in [Vector2(-1, 3), Vector2(29, 3), Vector2(-2, 18), Vector2(29, 18), Vector2(5, -2), Vector2(18, -2)]:
        var tc := _iso(tree)
        draw_line(tc, tc + Vector2(0, -18), Color("#55412d"), 3.0)
        draw_circle(tc + Vector2(0, -25), 12.0, Color("#315c3c"))
        draw_circle(tc + Vector2(-7, -20), 8.0, Color("#4a7648"))
    var dock := _iso(Vector2(23, 5))
    draw_rect(Rect2(dock + Vector2(-24, -23), Vector2(48, 25)), Color("#806348"))
    draw_rect(Rect2(dock + Vector2(-15, -31), Vector2(30, 8)), Color("#b89467"))
    draw_string(ThemeDB.fallback_font, dock + Vector2(-33, 16), "RECEIVING", HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color("#342b23"))
    draw_string(ThemeDB.fallback_font, _iso(Vector2(4, geometry_height + 4)), "PARKING", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color("#ded6bd"))
    draw_string(ThemeDB.fallback_font, _iso(Vector2(21, geometry_height + 6)), "ROAD", HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color("#d4d7da"))

func _draw_floor() -> void:
    # Drop shadow and curb line give the building mass.
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside(x,y): continue
            var c := _iso(Vector2(x,y))
            draw_colored_polygon(_diamond(c+Vector2(7,8)), Color(0.02,0.03,0.04,0.25))
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside(x,y): continue
            var c := _iso(Vector2(x,y))
            draw_colored_polygon(_diamond(c), _floor_color(x,y))
            # Suppress most grid noise; only subtle seams remain.
            draw_polyline(PackedVector2Array([c+Vector2(0,-10),c+Vector2(20,0),c+Vector2(0,10),c+Vector2(-20,0),c+Vector2(0,-10)]), Color(0.25,0.22,0.18,0.10), 0.55)
            if x >= 3 and x <= 18 and y % 4 == 0: draw_line(c + Vector2(-8, 0), c + Vector2(8, 0), Color(0.72,0.64,0.48,0.18), 1.2)

func _room_color(room_type: int) -> Color:
    var colors := {1:Color("#d4b06b"), 2:Color("#8fa69a"), 3:Color("#807b76"), 4:Color("#8a7468"), 5:Color("#756b83"), 6:Color("#6c9aa5")}
    return colors.get(room_type, Color("#9a9285"))

func _room_name(room_type: int) -> String:
    var names := {1:"SALES FLOOR", 2:"STOCKROOM", 3:"RECEIVING", 4:"OFFICE", 5:"SECURITY", 6:"VESTIBULE"}
    return names.get(room_type, "ROOM")

func _draw_rooms() -> void:
    for room in rooms:
        var tint: Color = _room_color(int(room.type))
        for y in range(int(room.y), int(room.y + room.height)):
            for x in range(int(room.x), int(room.x + room.width)):
                if _inside(x, y): draw_colored_polygon(_diamond(_iso(Vector2(x, y)), 2.0), Color(tint, 0.09))
        var label_pos := _iso(Vector2(room.x + room.width * 0.5, room.y + room.height * 0.5))
        draw_string(ThemeDB.fallback_font, label_pos, _room_name(int(room.type)), HORIZONTAL_ALIGNMENT_CENTER, 110, 10, Color(tint, 0.62))
        var tl := _iso(Vector2(room.x, room.y)); var tr := _iso(Vector2(room.x + room.width, room.y))
        var br := _iso(Vector2(room.x + room.width, room.y + room.height)); var bl := _iso(Vector2(room.x, room.y + room.height))
        draw_polyline(PackedVector2Array([tl, tr, br, bl, tl]), Color(tint, 0.34), 2.0)

func _draw_building_edges() -> void:
    for y in range(geometry_height):
        for x in range(geometry_width):
            if not _inside(x, y): continue
            var c := _iso(Vector2(x, y))
            if not _inside(x, y - 1):
                draw_line(c + Vector2(-20, 0), c + Vector2(0, -10), Color("#d8c9aa"), 5.0)
                draw_line(c + Vector2(-20, 7), c + Vector2(0, -3), Color(0.08, 0.07, 0.06, 0.28), 8.0)
            if not _inside(x + 1, y): draw_line(c + Vector2(0, -10), c + Vector2(20, 0), Color("#66574b"), 6.0)
            if not _inside(x, y + 1): draw_line(c + Vector2(20, 0), c + Vector2(0, 10), Color("#51463d"), 6.0)
            if not _inside(x - 1, y): draw_line(c + Vector2(0, 10), c + Vector2(-20, 0), Color("#b29e82"), 5.0)

func _draw_walls() -> void:
    for w in walls:
        var a := _iso(w.a); var b := _iso(w.b)
        draw_line(a+Vector2(4,6), b+Vector2(4,6), Color(0.05,0.05,0.05,0.26), 10.0)
        draw_line(a, b, Color("#574c43"), 8.0)
        draw_line(a+Vector2(0,-4), b+Vector2(0,-4), Color("#d5c4a8"), 3.0)

func _draw_frontage() -> void:
    var entrance := _iso(Vector2(1, 10))
    draw_line(entrance + Vector2(-28, 8), entrance + Vector2(28, 8), Color("#9d8262"), 7.0)
    draw_line(entrance + Vector2(-24, 2), entrance + Vector2(24, 2), Color("#e4c985"), 3.0)
    draw_string(ThemeDB.fallback_font, entrance + Vector2(-42, -43), "SHRINK CITY MARKET", HORIZONTAL_ALIGNMENT_LEFT, -1, 13, Color("#f0d28c"))
    draw_circle(entrance + Vector2(-24, 12), 3.0, Color("#d6b86b"))
    draw_circle(entrance + Vector2(24, 12), 3.0, Color("#d6b86b"))

func _draw_fixtures() -> void:
    # Sort roughly by iso depth so foreground fixtures sit above background fixtures.
    var ordered := fixtures.duplicate()
    ordered.sort_custom(func(a,b): return int(a.x+a.y) < int(b.x+b.y))
    for f in ordered: _draw_fixture(f, 1.0)

func _fixture_color(kind: String) -> Color:
    for key in BUILD_DEFS:
        if BUILD_DEFS[key].type == kind: return BUILD_DEFS[key].color
    if kind == "entrance" or kind == "exit": return Color("#4d91a8")
    if kind == "locked_case": return Color("#7f6488")
    return Color("#6d7880")

func _draw_fixture(f: Dictionary, alpha: float) -> void:
    var kind := str(f.kind)
    var width := int(f.get("width", 1))
    var height := int(f.get("height", 1))
    if kind in ["shelf", "shelf_bin", "short_shelf", "locked_shelf", "clearance", "locked_case"]:
        for oy in range(height):
            for ox in range(width):
                var cell := _iso(Vector2(int(f.x) + ox, int(f.y) + oy))
                var color := Color("#bb4f4f") if bool(f.get("preview_invalid", false)) else _fixture_color(kind)
                var product := int(f.get("product", -1))
                if product == 0: color = color.lerp(Color("#6f9c62"), 0.28)
                elif product == 1: color = color.lerp(Color("#5f88a9"), 0.22)
                elif product == 2: color = color.lerp(Color("#b37955"), 0.18)
                color.a = alpha
                draw_colored_polygon(_diamond(cell), Color(color.darkened(0.42), 0.42 * alpha))
                draw_rect(Rect2(cell.x - 13, cell.y - 25, 26, 21), color)
                draw_rect(Rect2(cell.x - 11, cell.y - 21, 22, 3), color.lightened(0.20))
                draw_rect(Rect2(cell.x - 11, cell.y - 11, 22, 3), color.lightened(0.10))
        var badge := _iso(Vector2(int(f.x) + (width - 1) * 0.5, int(f.y) + (height - 1) * 0.5))
        if int(f.get("product", -1)) >= 0: _product_badge(int(f.product), badge + Vector2(0, -31), alpha)
    else:
        var p := _iso(Vector2(f.x, f.y))
        var color := _fixture_color(kind); color.a = alpha
        draw_colored_polygon(_diamond(p), Color(color.darkened(0.42), 0.42 * alpha))
        if kind in ["register", "self_checkout"]:
            draw_rect(Rect2(p.x - 17, p.y - 16, 34, 13), color.darkened(0.13))
            draw_rect(Rect2(p.x - 8, p.y - 29, 16, 14), color)
            draw_rect(Rect2(p.x - 5, p.y - 26, 10, 5), Color("#b7e1d7"))
        elif kind == "camera":
            draw_circle(p + Vector2(0, -18), 7, color)
            draw_line(p + Vector2(0, -3), p + Vector2(0, -22), Color("#dfe8ec"), 2.5)
            draw_arc(p + Vector2(0, -10), 56, -2.5, -0.65, 24, Color(0.38, 0.62, 0.82, 0.10 * alpha), 12.0)
        elif kind in ["entrance", "exit"]:
            draw_line(p + Vector2(-13, 1), p + Vector2(-13, -33), color, 5)
            draw_line(p + Vector2(13, 1), p + Vector2(13, -33), color, 5)
            draw_line(p + Vector2(-13, -33), p + Vector2(13, -33), color.lightened(0.15), 4)

func _product_badge(product: int, p: Vector2, alpha: float) -> void:
    var colors := [Color("#d78a42"),Color("#4b8eb1"),Color("#d15c70"),Color("#7b9e56")]
    var c: Color = colors[product % colors.size()]; c.a = alpha
    draw_circle(p,5,c)

func _draw_people() -> void:
    var ordered := entities.values()
    ordered.sort_custom(func(a,b): return float(a.x+a.y) < float(b.x+b.y))
    for e in ordered:
        var p := _iso(Vector2(e.x,e.y))
        draw_ellipse_shadow(p)
        var variant := int(e.id) % 6
        _sprite_region(variant,p,Vector2(34,43))

func _draw_employees() -> void:
    for employee in employees:
        var p := _iso(Vector2(int(employee.x), int(employee.y)))
        draw_circle(p + Vector2(0, -16), 6.0, Color("#f0a34a"))
        draw_rect(Rect2(p.x - 6, p.y - 12, 12, 12), Color("#df7d3d"))
        if int(employee.role) == 4:
            draw_line(p + Vector2(-8, -7), p + Vector2(8, -7), Color("#6e4c9b"), 3.0)

func draw_ellipse_shadow(p: Vector2) -> void:
    draw_circle(p+Vector2(0,-1),10,Color(0.03,0.04,0.04,0.28))

func _sprite_region(region: int, p: Vector2, size: Vector2) -> void:
    if sprite_sheet == null: return
    var cell := Vector2(float(sprite_sheet.get_width())/4.0,float(sprite_sheet.get_height())/3.0)
    var source := Rect2(Vector2(region%4,region/4)*cell,cell)
    draw_texture_rect_region(sprite_sheet,Rect2(p-Vector2(size.x*0.5,size.y),size),source)

func _preview_size(kind: String, rotation: int) -> Vector2i:
    var sizes := {"shelf":Vector2i(3,1), "shelf_bin":Vector2i(1,1), "short_shelf":Vector2i(2,1), "locked_shelf":Vector2i(2,1), "clearance":Vector2i(2,1), "register":Vector2i(1,1), "self_checkout":Vector2i(1,1), "camera":Vector2i(1,1)}
    var size: Vector2i = sizes.get(kind, Vector2i(1,1))
    return Vector2i(size.y, size.x) if rotation % 2 == 1 else size

func _preview_is_valid(tile: Vector2, kind: String, rotation: int) -> bool:
    var size := _preview_size(kind, rotation)
    for oy in range(size.y):
        for ox in range(size.x):
            var x := int(tile.x) + ox; var y := int(tile.y) + oy
            if not _inside(x, y): return false
            for f in fixtures:
                var fw := int(f.get("width", 1)); var fh := int(f.get("height", 1))
                if x >= int(f.x) and x < int(f.x) + fw and y >= int(f.y) and y < int(f.y) + fh: return false
    return true

func _draw_build_preview() -> void:
    if not build_mode or build_cursor.x < 0: return
    var key: String = BUILD_ORDER[clamp(BUILD_ORDER.find(build_kind),0,BUILD_ORDER.size()-1)] if build_kind in BUILD_ORDER else "shelf"
    var def: Dictionary = BUILD_DEFS[key]
    var size := _preview_size(def.type, build_rotation)
    var valid := _preview_is_valid(build_cursor, def.type, build_rotation)
    _draw_fixture({"x":int(build_cursor.x),"y":int(build_cursor.y),"kind":def.type,"product":-1,"width":size.x,"height":size.y,"preview_invalid":not valid},0.52)

func _draw_hud() -> void:
    # Top command bar
    draw_rect(Rect2(0,0,1280,58),Color("#f2eee2"))
    draw_string(ThemeDB.fallback_font,Vector2(24,36),"SHRINK CITY",HORIZONTAL_ALIGNMENT_LEFT,-1,25,Color("#294047"))
    draw_string(ThemeDB.fallback_font,Vector2(185,34),"DAY 1  •  LAYOUT %d  •  %d shoppers" % [layout_id + 1, entities.size()],HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#587078"))
    _metric_chip(Vector2(430,12),"SALES","$%.0f" % metrics.get("revenue",0.0),Color("#79b986"))
    _metric_chip(Vector2(560,12),"SHRINK","$%.0f" % metrics.get("shrink",0.0),Color("#d87870"))
    _metric_chip(Vector2(690,12),"WAIT","%.0fs" % metrics.get("wait",0.0),Color("#d4b367"))
    _metric_chip(Vector2(820,12),"SAT","%.0f%%" % metrics.get("satisfaction",100.0),Color("#54a5ad"))
    _metric_chip(Vector2(950,12),"STAFF","%d" % employees.size(),Color("#d28b4e"))

    # Inspector / store summary
    draw_rect(Rect2(1018,70,250,555),Color("#edf3ed"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,103),"STORE MANAGER",HORIZONTAL_ALIGNMENT_LEFT,-1,18,Color("#30464b"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,130),"Operating snapshot",HORIZONTAL_ALIGNMENT_LEFT,-1,12,Color("#60787d"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,174),"Revenue",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#60787d"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,174),"$%.2f" % metrics.get("revenue",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#d9ead7"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,202),"Labor",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#60787d"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,202),"$%.2f" % metrics.get("labor",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#e7ddd0"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,230),"Shrink",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#60787d"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,230),"$%.2f" % metrics.get("shrink",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#e6a19d"))
    draw_line(Vector2(1038,252),Vector2(1245,252),Color("#b8c9c0"),1)
    draw_string(ThemeDB.fallback_font,Vector2(1038,286),"INSPECTOR",HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#30464b"))
    if selected.is_empty():
        draw_multiline_string(ThemeDB.fallback_font,Vector2(1038,316),"Select a shopper or fixture.\n\nDrag to pan • wheel to zoom\nB toggles build mode\n1–8 picks fixture",HORIZONTAL_ALIGNMENT_LEFT,200,13,21,Color("#587078"))
    else:
        var txt := "%s\nID %s\nTile %s, %s" % [selected.get("kind","Shopper").capitalize(),selected.get("id","—"),selected.get("x","—"),selected.get("y","—")]
        if selected.has("product") and int(selected.get("product", -1)) >= 0: txt += "\nProduct: %s" % [PRODUCT_SYMBOLS[int(selected.product)]]
        if selected.has("width"): txt += "\nFootprint: %sx%s" % [selected.width, selected.height]
        if selected.has("target_fixture_id"): txt += "\nTarget fixture %s\nAccess cell %s, %s" % [selected.target_fixture_id, selected.target_x, selected.target_y]
        if selected.has("archetype"): txt += "\nArchetype %s\nBudget $%.2f\nPatience %.0fs" % [selected.archetype, selected.budget, selected.patience]
        draw_multiline_string(ThemeDB.fallback_font,Vector2(1038,316),txt,HORIZONTAL_ALIGNMENT_LEFT,200,14,23,Color("#30464b"))

    # Bottom build palette: much closer to classic tycoon controls than debug hotkey text.
    draw_rect(Rect2(18,636,982,68),Color("#e7efe8"))
    draw_string(ThemeDB.fallback_font,Vector2(30,660),"BUILD" if build_mode else "TOOLS",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#9fb2bd"))
    for i in range(BUILD_ORDER.size()):
        var key: String = BUILD_ORDER[i]
        var d: Dictionary = BUILD_DEFS[key]
        var r := Rect2(92+i*108,646,98,46)
        var active := build_mode and build_kind == key
        draw_rect(r,Color("#38515d") if active else Color("#24343d"))
        draw_rect(Rect2(r.position,Vector2(5,r.size.y)),d.color)
        draw_string(ThemeDB.fallback_font,r.position+Vector2(12,18),"%d  %s" % [i+1,d.name],HORIZONTAL_ALIGNMENT_LEFT,82,11,Color("#30464b"))
        draw_string(ThemeDB.fallback_font,r.position+Vector2(12,36),"$%d" % d.cost,HORIZONTAL_ALIGNMENT_LEFT,82,10,Color("#60787d"))
    if feedback_time > 0.0: draw_string(ThemeDB.fallback_font,Vector2(1038,600),command_feedback,HORIZONTAL_ALIGNMENT_LEFT,205,12,Color("#8bd19a") if command_feedback == "Construction accepted" else Color("#e57e76"))
    elif process_error != "": draw_string(ThemeDB.fallback_font,Vector2(1038,600),process_error,HORIZONTAL_ALIGNMENT_LEFT,205,12,Color("#e57e76"))

func _metric_chip(pos: Vector2, label: String, value: String, accent: Color) -> void:
    draw_rect(Rect2(pos,Vector2(116,34)),Color("#dbe8df"))
    draw_rect(Rect2(pos,Vector2(4,34)),accent)
    draw_string(ThemeDB.fallback_font,pos+Vector2(12,14),label,HORIZONTAL_ALIGNMENT_LEFT,-1,9,Color("#8fa3ad"))
    draw_string(ThemeDB.fallback_font,pos+Vector2(12,29),value,HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#eee6d8"))

func _send(command: String) -> void:
    if sim_pipe != null:
        sim_pipe.store_line(command); sim_pipe.flush()

func _input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed:
            zoom = clamp(zoom * 1.10, 0.65, 1.45)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed:
            zoom = clamp(zoom / 1.10, 0.65, 1.45)
        elif event.button_index == MOUSE_BUTTON_LEFT:
            if event.pressed:
                if event.position.y >= 636 and event.position.x < 1000:
                    var idx := int((event.position.x - 92.0) / 108.0)
                    if idx >= 0 and idx < BUILD_ORDER.size(): build_kind = BUILD_ORDER[idx]; build_mode = true
                elif event.position.x < WORLD_PANEL_RIGHT:
                    var tile := _grid(event.position)
                    var fixture := _fixture_at_tile(tile)
                    if build_mode:
                        if _inside(int(tile.x), int(tile.y)):
                            var def: Dictionary = BUILD_DEFS[build_kind]
                            _send("PLACE %s %d %d %d" % [def.type, int(tile.x), int(tile.y), build_rotation])
                    elif not fixture.is_empty():
                        selected = fixture
                        dragged_id = int(fixture.id)
                        dragged_fixture = true
                    else:
                        dragging = true
                        _select_at(event.position)
            else:
                if dragged_fixture and dragged_id != 0:
                    var tile := _grid(event.position)
                    _send("MOVE %d %d %d" % [dragged_id, int(tile.x), int(tile.y)])
                dragged_fixture = false; dragged_id = 0; dragging = false
        elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed and event.position.x < WORLD_PANEL_RIGHT:
            var fixture := _fixture_at_tile(_grid(event.position))
            if fixture != null: _send("REMOVE %d" % int(fixture.id))
    elif event is InputEventMouseMotion:
        mouse_position = event.position
        build_cursor = _grid(event.position)
        if dragged_fixture:
            var tile := _grid(event.position)
            if _inside(int(tile.x), int(tile.y)):
                for f in fixtures:
                    if int(f.id) == dragged_id: f.x = int(tile.x); f.y = int(tile.y); selected = f; break
        elif dragging:
            view_offset += event.relative
            view_offset.x = clamp(view_offset.x, -90.0, 125.0)
            view_offset.y = clamp(view_offset.y, 25.0, 115.0)
    elif event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_B: build_mode = not build_mode
        elif event.keycode == KEY_ESCAPE: build_mode = false; dragging = false; dragged_fixture = false; selected = {}
        elif event.keycode == KEY_Q: build_rotation = (build_rotation + 3) % 4
        elif event.keycode == KEY_E: build_rotation = (build_rotation + 1) % 4
        elif event.keycode == KEY_F3: debug_overlay = not debug_overlay
        elif event.keycode >= KEY_1 and event.keycode <= KEY_8:
            var idx := int(event.keycode - KEY_1)
            build_kind = BUILD_ORDER[idx]; build_mode = true
        elif event.keycode == KEY_R:
            view_offset = Vector2(45, 70); zoom = 1.0
            command_feedback = ""; process_error = ""
        queue_redraw()

func _fixture_at_tile(tile: Vector2) -> Dictionary:
    for f in fixtures:
        var width := int(f.get("width", 1)); var height := int(f.get("height", 1))
        if int(tile.x) >= int(f.x) and int(tile.x) < int(f.x) + width and int(tile.y) >= int(f.y) and int(tile.y) < int(f.y) + height: return f
    return {}

func _select_at(pos: Vector2) -> void:
    selected = {}
    var fixture := _fixture_at_tile(_grid(pos))
    if not fixture.is_empty(): selected = fixture
    var best := 28.0
    for e in entities.values():
        var d := _screen(Vector2(e.x, e.y)).distance_to(pos)
        if d < best: best = d; selected = {"id":e.id,"kind":"shopper","x":snapped(e.x,0.1),"y":snapped(e.y,0.1),"product":e.product,"target_fixture_id":e.target_fixture_id,"target_x":e.target_x,"target_y":e.target_y,"archetype":e.archetype,"budget":e.budget,"patience":e.patience}

func _draw_debug_overlay() -> void:
    for y in range(geometry_height):
        for x in range(geometry_width):
            if _inside(x, y):
                var c := _iso(Vector2(x, y))
                draw_string(ThemeDB.fallback_font, c + Vector2(-5, 3), "%d,%d" % [x, y], HORIZONTAL_ALIGNMENT_LEFT, -1, 6, Color(0.1,0.1,0.1,0.42))
    for e in entities.values():
        if e.get("target_fixture_id", 0) != 0:
            draw_line(_iso(Vector2(e.x, e.y)), _iso(Vector2(e.target_x, e.target_y)), Color("#ffcf66"), 1.5)
