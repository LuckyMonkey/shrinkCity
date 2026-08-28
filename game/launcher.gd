extends Control

const SCENARIOS := [
    {"slug":"corner-market", "title":"CORNER MARKET", "subtitle":"First Shift", "blurb":"Compact grocery. Tight margins, opportunistic theft, and a lean crew.", "goal":"Goal: build profit without choking the front end.", "difficulty":"★☆☆"},
    {"slug":"electronics", "title":"ELECTRONICS", "subtitle":"High Value", "blurb":"Portable expensive goods, locked cases, cameras, and costly LP coverage.", "goal":"Goal: hold shrink down without driving shoppers away.", "difficulty":"★★★"},
    {"slug":"big-box", "title":"BIG BOX", "subtitle":"Saturday Rush", "blurb":"Heavy traffic, self-checkout pressure, staffing tradeoffs, and more moving parts.", "goal":"Goal: serve the rush without losing the store to queues or shrink.", "difficulty":"★★★"},
    {"slug":"pharmacy", "title":"PHARMACY", "subtitle":"Small Items, Big Risk", "blurb":"Small concealable goods and security choices where friction matters.", "goal":"Goal: protect high-risk categories while preserving satisfaction.", "difficulty":"★★☆"}
]

var selected_index := 0
var title_label: Label
var subtitle_label: Label
var blurb_label: Label
var goal_label: Label
var difficulty_label: Label
var scenario_buttons: Array[Button] = []
var rotate_timer: Timer

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    mouse_filter = Control.MOUSE_FILTER_PASS
    _build_ui()
    _select_scenario(0, false)
    rotate_timer = Timer.new()
    rotate_timer.wait_time = 22.0
    rotate_timer.autostart = true
    rotate_timer.timeout.connect(_rotate_demo)
    add_child(rotate_timer)

func _build_ui() -> void:
    var shade := ColorRect.new()
    shade.color = Color(0.93, 0.96, 0.92, 0.72)
    shade.position = Vector2(0, 0)
    shade.size = Vector2(1280, 720)
    shade.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(shade)

    var panel := PanelContainer.new()
    panel.position = Vector2(34, 82)
    panel.size = Vector2(410, 548)
    add_child(panel)

    var margin := MarginContainer.new()
    margin.add_theme_constant_override("margin_left", 24)
    margin.add_theme_constant_override("margin_right", 24)
    margin.add_theme_constant_override("margin_top", 22)
    margin.add_theme_constant_override("margin_bottom", 22)
    panel.add_child(margin)

    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 12)
    margin.add_child(column)

    var brand := Label.new()
    brand.text = "SHRINK CITY"
    brand.add_theme_font_size_override("font_size", 30)
    column.add_child(brand)

    var kicker := Label.new()
    kicker.text = "CHOOSE A STOREFRONT  •  LIVE DEMO RUNNING"
    kicker.modulate = Color("#577078")
    column.add_child(kicker)

    column.add_child(HSeparator.new())

    var buttons := HBoxContainer.new()
    buttons.add_theme_constant_override("separation", 6)
    column.add_child(buttons)
    for i in range(SCENARIOS.size()):
        var button := Button.new()
        button.text = str(i + 1)
        button.custom_minimum_size = Vector2(62, 44)
        button.tooltip_text = SCENARIOS[i]["title"]
        button.pressed.connect(_select_scenario.bind(i, true))
        buttons.add_child(button)
        scenario_buttons.append(button)

    title_label = Label.new()
    title_label.add_theme_font_size_override("font_size", 25)
    column.add_child(title_label)

    subtitle_label = Label.new()
    subtitle_label.add_theme_font_size_override("font_size", 17)
    subtitle_label.modulate = Color("#4d7377")
    column.add_child(subtitle_label)

    difficulty_label = Label.new()
    difficulty_label.modulate = Color("#b56c35")
    column.add_child(difficulty_label)

    blurb_label = Label.new()
    blurb_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    blurb_label.custom_minimum_size = Vector2(350, 68)
    column.add_child(blurb_label)

    goal_label = Label.new()
    goal_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    goal_label.modulate = Color("#4b5c42")
    goal_label.custom_minimum_size = Vector2(350, 50)
    column.add_child(goal_label)

    var hint := Label.new()
    hint.text = "The store behind this menu is the real C simulation.\nIncidents, shoppers, guards, queues and losses are live."
    hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    hint.modulate = Color("#667477")
    column.add_child(hint)

    var spacer := Control.new()
    spacer.custom_minimum_size = Vector2(1, 16)
    column.add_child(spacer)

    var play := Button.new()
    play.text = "RUN THIS STORE"
    play.custom_minimum_size = Vector2(350, 52)
    play.pressed.connect(_start_selected)
    column.add_child(play)

    var demo := Button.new()
    demo.text = "KEEP WATCHING ATTRACT MODE"
    demo.custom_minimum_size = Vector2(350, 42)
    demo.pressed.connect(_hide_menu)
    column.add_child(demo)

    var footer := Label.new()
    footer.text = "Esc: scenario menu  •  1-4: switch demo storefront"
    footer.modulate = Color("#748489")
    column.add_child(footer)

func _demo_node() -> Node:
    return get_node_or_null("../Demo")

func _select_scenario(index: int, restart_demo: bool) -> void:
    selected_index = clamp(index, 0, SCENARIOS.size() - 1)
    var scenario: Dictionary = SCENARIOS[selected_index]
    title_label.text = scenario["title"]
    subtitle_label.text = scenario["subtitle"]
    difficulty_label.text = "Difficulty  " + scenario["difficulty"]
    blurb_label.text = scenario["blurb"]
    goal_label.text = scenario["goal"]
    for i in range(scenario_buttons.size()):
        scenario_buttons[i].disabled = i == selected_index
    if restart_demo:
        var demo := _demo_node()
        if demo != null and demo.has_method("start_scenario"):
            demo.start_scenario(scenario["slug"])
        if rotate_timer != null:
            rotate_timer.start()

func _start_selected() -> void:
    var scenario: Dictionary = SCENARIOS[selected_index]
    var demo := _demo_node()
    if demo != null and demo.has_method("start_scenario"):
        demo.start_scenario(scenario["slug"])
    _hide_menu()
    if rotate_timer != null:
        rotate_timer.stop()

func _hide_menu() -> void:
    visible = false

func _rotate_demo() -> void:
    if not visible:
        return
    _select_scenario((selected_index + 1) % SCENARIOS.size(), true)

func _unhandled_input(event: InputEvent) -> void:
    if event.is_action_pressed("ui_cancel"):
        visible = not visible
        if visible and rotate_timer != null:
            rotate_timer.start()
        elif rotate_timer != null:
            rotate_timer.stop()
        get_viewport().set_input_as_handled()
        return
    if visible and event is InputEventKey and event.pressed and not event.echo:
        var key := event as InputEventKey
        if key.keycode >= KEY_1 and key.keycode <= KEY_4:
            _select_scenario(int(key.keycode - KEY_1), true)
            get_viewport().set_input_as_handled()
