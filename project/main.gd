# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
extends Control
const Tokens = preload("res://tokens.gd")
# Console screen: GPU, guest and call log on the left and centre, the design tokens below.
# The C node reaches this node as the engine singleton "Console".

var log_view: RichTextLabel
var guest_value: Label
var calls_value: Label
var calls := 0
var mono: Font

func _ready() -> void:
	mono = SystemFont.new()
	(mono as SystemFont).font_names = PackedStringArray(["DejaVu Sans Mono", "monospace"])
	Engine.register_singleton("Console", self)
	var bg := ColorRect.new()
	bg.color = Tokens.SURFACE_0
	bg.set_anchors_preset(Control.PRESET_FULL_RECT)
	add_child(bg)
	var root := VBoxContainer.new()
	root.set_anchors_preset(Control.PRESET_FULL_RECT)
	root.offset_left = 16; root.offset_top = 16; root.offset_right = -16; root.offset_bottom = -16
	root.add_theme_constant_override("separation", 12)
	add_child(root)
	root.add_child(_header())
	var row := HBoxContainer.new()
	row.size_flags_vertical = Control.SIZE_EXPAND_FILL
	row.add_theme_constant_override("separation", 12)
	root.add_child(row)
	var left := VBoxContainer.new()
	left.custom_minimum_size.x = 380
	left.add_theme_constant_override("separation", 12)
	row.add_child(left)
	left.add_child(_panel("GPU", _gpu_rows()))
	guest_value = _value("none loaded", Tokens.WARN)
	calls_value = _value("0")
	left.add_child(_panel("GUEST", [_pair("elf", guest_value), _pair("vmcalls", calls_value)]))
	log_view = RichTextLabel.new()
	log_view.bbcode_enabled = true
	log_view.scroll_following = true
	log_view.add_theme_font_override("normal_font", mono)
	log_view.add_theme_font_size_override("normal_font_size", Tokens.TYPE_SIZES["body"])
	log_view.add_theme_color_override("default_color", Tokens.TEXT)
	var log_panel := _panel("CALL LOG", [log_view])
	log_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(log_panel)
	root.add_child(_tokens_panel())
	log_line("console ready", "ok")

func log_line(text: String, level: String = "info") -> void:
	var colour := {"ok": Tokens.OK, "warn": Tokens.WARN, "fail": Tokens.FAIL, "info": Tokens.TEXT}.get(level, Tokens.TEXT) as Color
	log_view.append_text("[color=#%s]%s[/color]  [color=#%s]%s[/color]\n" % [Tokens.TEXT_DIM.to_html(false), Time.get_time_string_from_system(), colour.to_html(false), text.replace("[", "[lb]")])

func gpu_info() -> String:
	return "%s | rd %s | %s" % [RenderingServer.get_video_adapter_name(), "present" if RenderingServer.get_rendering_device() != null else "missing", DisplayServer.get_name()]

func set_guest(name: String, size: int) -> void:
	guest_value.text = "%s  %d bytes" % [name, size]
	guest_value.add_theme_color_override("font_color", Tokens.OK)

func count_call() -> void:
	calls += 1
	calls_value.text = str(calls)

func _label(text: String, size: int, colour: Color) -> Label:
	var l := Label.new()
	l.text = text
	l.add_theme_font_override("font", mono)
	l.add_theme_font_size_override("font_size", size)
	l.add_theme_color_override("font_color", colour)
	return l

func _value(text: String, colour: Color = Tokens.TEXT) -> Label:
	return _label(text, Tokens.TYPE_SIZES["body"], colour)

func _pair(key: String, value: Label) -> Control:
	var h := HBoxContainer.new()
	var k := _label(key, Tokens.TYPE_SIZES["label"], Tokens.TEXT_DIM)
	k.custom_minimum_size.x = 96
	h.add_child(k)
	h.add_child(value)
	return h

func _panel(title: String, children: Array) -> PanelContainer:
	var p := PanelContainer.new()
	var style := StyleBoxFlat.new()
	style.bg_color = Tokens.SURFACE_1
	style.border_color = Tokens.LINE
	style.set_border_width_all(1)
	style.set_corner_radius_all(2)
	style.set_content_margin_all(Tokens.SPACE[2])
	p.add_theme_stylebox_override("panel", style)
	var v := VBoxContainer.new()
	v.add_theme_constant_override("separation", Tokens.SPACE[1])
	p.add_child(v)
	v.add_child(_label(title, Tokens.TYPE_SIZES["label"], Tokens.ACCENT))
	for c in children:
		(c as Control).size_flags_vertical = Control.SIZE_EXPAND_FILL if c is RichTextLabel else Control.SIZE_SHRINK_BEGIN
		v.add_child(c)
	return p

func _header() -> Control:
	var h := HBoxContainer.new()
	h.add_child(_label("GUEST-CNODE", Tokens.TYPE_SIZES["title"], Tokens.TEXT))
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	h.add_child(spacer)
	h.add_child(_label("libgodot  %s" % Engine.get_version_info()["string"], Tokens.TYPE_SIZES["label"], Tokens.TEXT_DIM))
	return h

func _gpu_rows() -> Array:
	var rd := RenderingServer.get_rendering_device()
	var adapter := RenderingServer.get_video_adapter_name()
	var rows := [
		_pair("adapter", _value(adapter, Tokens.OK if adapter != "" else Tokens.FAIL)),
		_pair("vendor", _value(RenderingServer.get_video_adapter_vendor())),
		_pair("api", _value(RenderingServer.get_video_adapter_api_version())),
		_pair("rd", _value("present" if rd != null else "missing", Tokens.OK if rd != null else Tokens.FAIL)),
		_pair("display", _value(DisplayServer.get_name())),
	]
	return rows

func _tokens_panel() -> Control:
	var h := HBoxContainer.new()
	h.add_theme_constant_override("separation", Tokens.SPACE[1])
	for key in Tokens.SWATCHES:
		var v := VBoxContainer.new()
		var s := ColorRect.new()
		s.color = Tokens.SWATCHES[key]
		s.custom_minimum_size = Vector2(88, 28)
		v.add_child(s)
		v.add_child(_label(key, Tokens.TYPE_SIZES["label"], Tokens.TEXT_DIM))
		h.add_child(v)
	var scale := VBoxContainer.new()
	for key in Tokens.TYPE_SIZES:
		scale.add_child(_label("%s %d" % [key, Tokens.TYPE_SIZES[key]], Tokens.TYPE_SIZES[key], Tokens.TEXT))
	h.add_child(scale)
	return _panel("DESIGN TOKENS", [h])
