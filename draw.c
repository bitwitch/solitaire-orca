static void draw_card(Card *card) {
	oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
	if (card->face_up) {
		oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
	} else {
		oc_image_draw(game.card_backs[game.selected_card_back], dest);
	}
	// draw outline around card
	oc_set_color_rgba(0.1, 0.1, 0.1, 0.69);
	oc_set_width(1);
	oc_rectangle_stroke(dest.x, dest.y, game.card_width, game.card_height);
}

static void draw_stock(void) {
	u32 border_width = 2;
	oc_set_width(border_width);
	oc_set_color_rgba(0.42, 0.42, 0.42, 0.69); // empty pile outline color
	oc_rounded_rectangle_stroke(
		game.stock.pos.x + (0.5f * border_width), 
		game.stock.pos.y + (0.5f * border_width), 
		game.card_width - border_width, 
		game.card_height - border_width,
		5);

	if (oc_list_empty(game.stock.cards)) {
		oc_rect dest = { game.stock.pos.x, game.stock.pos.y, game.card_width, game.card_height };
		oc_image_draw(game.reload_icon, dest);
	} else {
		oc_list_for_reverse(game.stock.cards, card, Card, node) {
			draw_card(card);
		}
	}
}

static void draw_waste(void) {
	oc_list_for_reverse(game.waste.cards, card, Card, node) {
		if (game.card_dragging == card) break;
		draw_card(card);
	}
}

static void draw_foundations(void) {
	// draw empty pile outlines
	u32 border_width = 2;
	oc_set_width(border_width);
	oc_set_color_rgba(0.69, 0.69, 0.69, 0.69);
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		oc_rounded_rectangle_stroke(
			game.foundations[i].pos.x + (0.5f * border_width), 
			game.foundations[i].pos.y + (0.5f * border_width), 
			game.card_width - border_width, 
			game.card_height - border_width,  
			5);
	}

	// draw cards
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		oc_list_for_reverse(game.foundations[i].cards, card, Card, node) {
			if (game.card_dragging == card) break;
			draw_card(card);
		}
	}
}

static void draw_tableau(void) {
	// draw empty pile outlines
	u32 border_width = 2;
	oc_set_width(border_width);
	oc_set_color_rgba(0.42, 0.42, 0.42, 0.69);
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		oc_rounded_rectangle_stroke(
			game.tableau[i].pos.x + (0.5f * border_width), 
			game.tableau[i].pos.y + (0.5f * border_width), 
			game.card_width - border_width, 
			game.card_height - border_width, 
			5);
	}

	// draw cards
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		oc_list_for_reverse(game.tableau[i].cards, card, Card, node) {
			if (game.card_dragging == card) break;
			draw_card(card);
		}
	}
}

static void draw_dragging(void) {
	if (!game.card_dragging) return;

	for (oc_list_elt *node = &game.card_dragging->node; node; node = oc_list_prev(node)) {
		Card *card = oc_list_entry(node, Card, node);
		draw_card(card);
	}
}

static void draw_test_deck(Card *cards, i32 num_cards) {
	assert(SUIT_COUNT * CARD_KIND_COUNT == num_cards);
	u32 card_width = 56;
	u32 card_height = 78;
	for (i32 suit=0; suit < SUIT_COUNT; ++suit)
	for (i32 kind=0; kind < CARD_KIND_COUNT; ++kind) {
		oc_rect dest = {
			.x = kind * card_width,
			.y = suit * card_height,
			.w = card_width, 
			.h = card_height };
		oc_image_draw_region(game.spritesheet, game.card_sprite_rects[suit][kind], dest);
	}

	for (i32 i=0; i<ARRAY_COUNT(game.card_backs); ++i) {
		oc_rect dest = {
			.x = i * card_width,
			.y = SUIT_COUNT * card_height,
			.w = card_width, 
			.h = card_height };
		oc_image_draw(game.card_backs[i], dest);
	}
}

oc_str8 WIN_TEXT = OC_STR8_LIT("YOU WIN!");

static void draw_win_text(void) {
	f32 font_size = 64;
	f32 padding = 50;
	oc_set_font(game.font);
	oc_set_font_size(font_size);
	oc_text_metrics metrics = oc_font_text_metrics(game.font, font_size, WIN_TEXT);
	f32 x = 0.5f * (game.frame_size.x - metrics.ink.w);
	f32 y = 0.5f * (game.frame_size.y + metrics.ink.h);

	oc_set_color_rgba(0, 0, 0, 0.75); 
	oc_rectangle_fill(
		x - padding, 
		y - metrics.ink.h - padding, 
		metrics.ink.w + 2.0f * padding, 
		metrics.ink.h + 2.0f * padding);

	oc_set_color_rgba(1, 1, 1, 1);
	oc_text_fill(x, y, WIN_TEXT);
}

static void solitaire_draw(void) {
    oc_canvas_select(game.canvas);
	oc_surface_select(game.surface);

	switch (game.state) {
	case STATE_SHOW_RULES: {
		oc_set_should_clear(true);
		oc_set_color(game.bg_color);
		oc_clear();
		oc_rect dest = {0, 0, game.frame_size.x, game.frame_size.y};
		oc_image_draw(game.rules_image, dest);
		break;
	}

	case STATE_TRANSITION_TO_WIN:
		oc_set_should_clear(true);
		oc_set_color(game.bg_color);
		oc_clear();
		draw_tableau();
		draw_foundations();
		break;

	case STATE_WIN: {
		oc_set_should_clear(false);
		Card *card = game.win_moving_card;
		if (card) {
			oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
			oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
		}
		break;
	}
		
	default:
		oc_set_should_clear(true);
		oc_set_color(game.bg_color);
		oc_clear();
		draw_waste();
		draw_stock();
		draw_tableau();
		draw_foundations();
		draw_dragging();
		break;
	}

	oc_ui_draw();
    oc_render(game.canvas);
    oc_surface_present(game.surface);
}

