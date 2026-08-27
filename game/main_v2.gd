extends Node2D

# Visual overhaul prototype. C remains authoritative for geometry and simulation.
var sim_pipe: FileAccess
var pending := ""
var process_error := ""
var entities: Dictionary = {}
var fixtures: Array[Dictionary] = []
var walls: Array[Dictionary] = []
var floor_rows: Array[int] = []
var metrics := {}
var geometry_width := 28
var geometry_height := 22
var last_tick := 0
var selected: Dictionary = {}
var hovered: Dictionary = {}
var mouse_position := Vector2.ZERO
var dragging := false
var view_offset := Vector2(165.0, 74.0)
var zoom := 1.06
var build_mode := false
var build_kind := "shelf"
var build_cursor := Vector2(-1, -1)
var sprite_sheet: Texture2D
var security_sprite: Texture2D

const ORIGIN := Vector2(430.0, 24.0)
const TILE_W := 40.0
const TILE_H := 20.0
const WORLD_PANEL_RIGHT := 1010.0
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
    var ticks := 999999
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

func _exit_tree() -> void:
    sim_pipe = null

func _process(_delta: float) -> void:
    if sim_pipe != null:
        var available := sim_pipe.get_length()
        if available > 0:
            pending += sim_pipe.get_buffer(available).get_string_from_utf8()
            var lines := pending.split("\n")
            pending = lines.pop_back()
            for line in lines: _consume_line(line)
    queue_redraw()

func _consume_line(line: String) -> void:
    var f := line.strip_edges().split(" ")
    if f.is_empty(): return
    match f[0]:
        "GEOMETRY":
            if f.size() >= 3:
                geometry_width = int(f[1]); geometry_height = int(f[2])
                floor_rows.clear(); fixtures.clear(); walls.clear()
                for _y in range(geometry_height): floor_rows.append(0)
        "FLOOR":
            if f.size() >= 3:
                var y := int(f[1])
                if y >= 0 and y < floor_rows.size(): floor_rows[y] = int(f[2])
        "WALL":
            if f.size() >= 6:
                walls.append({"id":int(f[1]), "a":Vector2(int(f[2]), int(f[3])), "b":Vector2(int(f[4]), int(f[5]))})
        "FIXTURE":
            if f.size() >= 6:
                fixtures.append({"id":int(f[1]), "kind":_kind_from_type(int(f[2])), "x":int(f[3]), "y":int(f[4]), "rotation":int(f[5]), "product":int(f[6]) if f.size() >= 7 else -1})
        "TICK":
            if f.size() >= 8:
                last_tick = int(f[1])
                metrics = {"revenue":float(f[3]), "shrink":float(f[4]), "labor":float(f[5]), "wait":float(f[6]), "satisfaction":float(f[7])}
                entities.clear()
        "ENTITY":
            if f.size() >= 6:
                entities[f[1]] = {"id":int(f[1]), "state":int(f[2]), "x":float(f[3]), "y":float(f[4]), "product":int(f[5])}
        "COMMAND":
            if f.size() >= 2 and int(f[1]) != 0: process_error = "Build rejected (%s)" % f[1]

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
    draw_rect(Rect2(0,0,1280,720), Color("#0d141a"))
    draw_set_transform(view_offset, 0.0, Vector2(zoom,zoom))
    _draw_site()
    _draw_floor()
    _draw_zone_markers()
    _draw_walls()
    _draw_fixtures()
    _draw_people()
    _draw_build_preview()
    draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
    _draw_hud()

func _draw_site() -> void:
    # A compact site apron makes the store feel like a property, not a floating debug diamond.
    for y in range(-2, geometry_height + 7):
        for x in range(-4, geometry_width + 5):
            if _inside(x,y): continue
            var c := _iso(Vector2(x,y))
            var color := Color("#526a4d")
            if y >= geometry_height + 1: color = Color("#444b50")
            elif x <= 1 and y >= 8 and y <= 16: color = Color("#8c8779")
            elif (x * 7 + y * 11) % 8 == 0: color = Color("#486244")
            draw_colored_polygon(_diamond(c), color)
    for x in range(1, 20, 3):
        var p := _iso(Vector2(x, geometry_height + 3))
        draw_line(p+Vector2(-13,0), p+Vector2(13,0), Color("#d9cf94"), 1.5)
    var road_a := _iso(Vector2(-2, geometry_height + 5))
    var road_b := _iso(Vector2(geometry_width + 3, geometry_height + 5))
    draw_line(road_a, road_b, Color("#777e81"), 2.0)

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

func _draw_zone_markers() -> void:
    _zone_label("FRESH + GROCERY", Vector2(5,3), Color("#765e39"))
    _zone_label("CENTER STORE", Vector2(8,10), Color("#6d5b43"))
    _zone_label("PROMO / SEASONAL", Vector2(5,18), Color("#665c55"))
    _zone_label("RECEIVING", Vector2(21,4), Color("#615c54"))
    _zone_label("BACK OF HOUSE", Vector2(21,15), Color("#5c5853"))

func _zone_label(text: String, grid_pos: Vector2, color: Color) -> void:
    var p := _iso(grid_pos)
    draw_string(ThemeDB.fallback_font, p, text, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(color,0.58))

func _draw_walls() -> void:
    for w in walls:
        var a := _iso(w.a); var b := _iso(w.b)
        draw_line(a+Vector2(4,6), b+Vector2(4,6), Color(0.05,0.05,0.05,0.26), 10.0)
        draw_line(a, b, Color("#574c43"), 8.0)
        draw_line(a+Vector2(0,-4), b+Vector2(0,-4), Color("#d5c4a8"), 3.0)

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
    var p := _iso(Vector2(f.x,f.y))
    var color := _fixture_color(str(f.kind)); color.a = alpha
    draw_colored_polygon(_diamond(p), Color(color.darkened(0.42), 0.42*alpha))
    if str(f.kind) in ["shelf","shelf_bin","short_shelf","locked_shelf","clearance","locked_case"]:
        draw_rect(Rect2(p.x-14,p.y-25,28,22), Color(0.04,0.04,0.04,0.22*alpha))
        draw_rect(Rect2(p.x-13,p.y-29,26,24), color)
        draw_rect(Rect2(p.x-11,p.y-25,22,4), color.lightened(0.20))
        draw_rect(Rect2(p.x-11,p.y-17,22,3), color.lightened(0.13))
        draw_rect(Rect2(p.x-11,p.y-9,22,3), color.lightened(0.08))
        if int(f.get("product",-1)) >= 0:
            _product_badge(int(f.product), p+Vector2(0,-34), alpha)
    elif str(f.kind) in ["register","self_checkout"]:
        draw_rect(Rect2(p.x-17,p.y-16,34,13), color.darkened(0.13))
        draw_rect(Rect2(p.x-8,p.y-29,16,14), color)
        draw_rect(Rect2(p.x-5,p.y-26,10,5), Color("#b7e1d7"))
    elif str(f.kind) == "camera":
        draw_circle(p+Vector2(0,-18), 7, color)
        draw_line(p+Vector2(0,-3), p+Vector2(0,-22), Color("#dfe8ec"), 2.5)
        draw_arc(p+Vector2(0,-10), 56, -2.5, -0.65, 24, Color(0.38,0.62,0.82,0.10*alpha), 12.0)
    elif str(f.kind) in ["entrance","exit"]:
        draw_line(p+Vector2(-13,1),p+Vector2(-13,-33),color,5)
        draw_line(p+Vector2(13,1),p+Vector2(13,-33),color,5)
        draw_line(p+Vector2(-13,-33),p+Vector2(13,-33),color.lightened(0.15),4)

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
        _sprite_region(variant,p,Vector2(44,54))
        _product_badge(int(e.product),p+Vector2(17,-43),1.0)

func draw_ellipse_shadow(p: Vector2) -> void:
    draw_circle(p+Vector2(0,-1),10,Color(0.03,0.04,0.04,0.28))

func _sprite_region(region: int, p: Vector2, size: Vector2) -> void:
    if sprite_sheet == null: return
    var cell := Vector2(float(sprite_sheet.get_width())/4.0,float(sprite_sheet.get_height())/3.0)
    var source := Rect2(Vector2(region%4,region/4)*cell,cell)
    draw_texture_rect_region(sprite_sheet,Rect2(p-Vector2(size.x*0.5,size.y),size),source)

func _draw_build_preview() -> void:
    if not build_mode or build_cursor.x < 0: return
    var key := BUILD_ORDER[clamp(BUILD_ORDER.find(build_kind),0,BUILD_ORDER.size()-1)] if build_kind in BUILD_ORDER else "shelf"
    var def: Dictionary = BUILD_DEFS[key]
    _draw_fixture({"x":int(build_cursor.x),"y":int(build_cursor.y),"kind":def.type,"product":-1},0.52)

func _draw_hud() -> void:
    # Top command bar
    draw_rect(Rect2(0,0,1280,58),Color("#121c23"))
    draw_string(ThemeDB.fallback_font,Vector2(24,36),"SHRINK CITY",HORIZONTAL_ALIGNMENT_LEFT,-1,25,Color("#f0e7d5"))
    draw_string(ThemeDB.fallback_font,Vector2(185,34),"DAY 1  •  %d shoppers" % entities.size(),HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#9fb2bd"))
    _metric_chip(Vector2(430,12),"SALES","$%.0f" % metrics.get("revenue",0.0),Color("#79b986"))
    _metric_chip(Vector2(560,12),"SHRINK","$%.0f" % metrics.get("shrink",0.0),Color("#d87870"))
    _metric_chip(Vector2(690,12),"WAIT","%.0fs" % metrics.get("wait",0.0),Color("#d4b367"))
    _metric_chip(Vector2(820,12),"SAT","%.0f%%" % metrics.get("satisfaction",100.0),Color("#7da8c4"))

    # Inspector / store summary
    draw_rect(Rect2(1018,70,250,555),Color("#18252e"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,103),"STORE MANAGER",HORIZONTAL_ALIGNMENT_LEFT,-1,18,Color("#f2eadb"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,130),"Operating snapshot",HORIZONTAL_ALIGNMENT_LEFT,-1,12,Color("#8fa4af"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,174),"Revenue",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#8fa4af"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,174),"$%.2f" % metrics.get("revenue",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#d9ead7"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,202),"Labor",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#8fa4af"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,202),"$%.2f" % metrics.get("labor",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#e7ddd0"))
    draw_string(ThemeDB.fallback_font,Vector2(1038,230),"Shrink",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#8fa4af"))
    draw_string(ThemeDB.fallback_font,Vector2(1180,230),"$%.2f" % metrics.get("shrink",0.0),HORIZONTAL_ALIGNMENT_RIGHT,68,16,Color("#e6a19d"))
    draw_line(Vector2(1038,252),Vector2(1245,252),Color("#33454f"),1)
    draw_string(ThemeDB.fallback_font,Vector2(1038,286),"INSPECTOR",HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#f2eadb"))
    if selected.is_empty():
        draw_multiline_string(ThemeDB.fallback_font,Vector2(1038,316),"Select a shopper or fixture.\n\nDrag to pan • wheel to zoom\nB toggles build mode\n1–8 picks fixture",HORIZONTAL_ALIGNMENT_LEFT,200,13,21,Color("#91a6b0"))
    else:
        var txt := "%s\nID %s\nTile %s, %s" % [selected.get("kind","Shopper").capitalize(),selected.get("id","—"),selected.get("x","—"),selected.get("y","—")]
        draw_multiline_string(ThemeDB.fallback_font,Vector2(1038,316),txt,HORIZONTAL_ALIGNMENT_LEFT,200,14,23,Color("#dfd4c3"))

    # Bottom build palette: much closer to classic tycoon controls than debug hotkey text.
    draw_rect(Rect2(18,636,982,68),Color("#17232b"))
    draw_string(ThemeDB.fallback_font,Vector2(30,660),"BUILD" if build_mode else "TOOLS",HORIZONTAL_ALIGNMENT_LEFT,-1,13,Color("#9fb2bd"))
    for i in range(BUILD_ORDER.size()):
        var key: String = BUILD_ORDER[i]
        var d: Dictionary = BUILD_DEFS[key]
        var r := Rect2(92+i*108,646,98,46)
        var active := build_mode and build_kind == key
        draw_rect(r,Color("#38515d") if active else Color("#24343d"))
        draw_rect(Rect2(r.position,Vector2(5,r.size.y)),d.color)
        draw_string(ThemeDB.fallback_font,r.position+Vector2(12,18),"%d  %s" % [i+1,d.name],HORIZONTAL_ALIGNMENT_LEFT,82,11,Color("#eee5d4"))
        draw_string(ThemeDB.fallback_font,r.position+Vector2(12,36),"$%d" % d.cost,HORIZONTAL_ALIGNMENT_LEFT,82,10,Color("#a9bac1"))
    if process_error != "": draw_string(ThemeDB.fallback_font,Vector2(1038,600),process_error,HORIZONTAL_ALIGNMENT_LEFT,205,12,Color("#e57e76"))

func _metric_chip(pos: Vector2, label: String, value: String, accent: Color) -> void:
    draw_rect(Rect2(pos,Vector2(116,34)),Color("#1e2d35"))
    draw_rect(Rect2(pos,Vector2(4,34)),accent)
    draw_string(ThemeDB.fallback_font,pos+Vector2(12,14),label,HORIZONTAL_ALIGNMENT_LEFT,-1,9,Color("#8fa3ad"))
    draw_string(ThemeDB.fallback_font,pos+Vector2(12,29),value,HORIZONTAL_ALIGNMENT_LEFT,-1,14,Color("#eee6d8"))

func _send(command: String) -> void:
    if sim_pipe != null:
        sim_pipe.store_line(command); sim_pipe.flush()

func _input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        if event.button_index == MOUSE_BUTTON_WHEEL_UP and event.pressed: zoom = clamp(zoom*1.10,0.65,1.75)
        elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN and event.pressed: zoom = clamp(zoom/1.10,0.65,1.75)
        elif event.button_index == MOUSE_BUTTON_LEFT:
            if event.pressed:
                if event.position.y >= 636 and event.position.x < 1000:
                    var idx := int((event.position.x-92.0)/108.0)
                    if idx >= 0 and idx < BUILD_ORDER.size(): build_kind = BUILD_ORDER[idx]; build_mode = true
                elif event.position.x < WORLD_PANEL_RIGHT:
                    dragging = true
                    if build_mode:
                        var tile := _grid(event.position)
                        if _inside(int(tile.x),int(tile.y)):
                            var def: Dictionary = BUILD_DEFS[build_kind]
                            _send("PLACE %s %d %d 0" % [def.type,int(tile.x),int(tile.y)])
                            dragging = false
                    else: _select_at(event.position)
            else: dragging = false
        elif event.button_index == MOUSE_BUTTON_RIGHT and event.pressed and event.position.x < WORLD_PANEL_RIGHT:
            var tile := _grid(event.position)
            for f in fixtures:
                if int(f.x)==int(tile.x) and int(f.y)==int(tile.y): _send("REMOVE %d" % int(f.id)); break
    elif event is InputEventMouseMotion:
        mouse_position = event.position
        build_cursor = _grid(event.position)
        if dragging and not build_mode: view_offset += event.relative
    elif event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_B: build_mode = not build_mode
        elif event.keycode == KEY_ESCAPE: build_mode = false; selected = {}
        elif event.keycode >= KEY_1 and event.keycode <= KEY_8:
            var idx := int(event.keycode-KEY_1)
            build_kind = BUILD_ORDER[idx]; build_mode = true
        elif event.keycode == KEY_R:
            view_offset = Vector2(165,74); zoom = 1.06

func _select_at(pos: Vector2) -> void:
    selected = {}
    var best := 28.0
    for f in fixtures:
        var d := _screen(Vector2(f.x,f.y)).distance_to(pos)
        if d < best: best = d; selected = f
    for e in entities.values():
        var d := _screen(Vector2(e.x,e.y)).distance_to(pos)
        if d < best: best = d; selected = {"id":e.id,"kind":"shopper","x":snapped(e.x,0.1),"y":snapped(e.y,0.1)}
