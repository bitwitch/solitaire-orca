#include <assert.h>
#include <orca.h>

#define CARD_ASPECT (560.0f/780.0f)
#define ARRAY_COUNT(array) sizeof(array) / sizeof(*(array))

typedef struct {
	oc_vec2 pos;
	oc_list cards;
} Pile;

typedef enum {
	CARD_ACE,
	CARD_TWO,
	CARD_THREE,
	CARD_FOUR,
	CARD_FIVE,
	CARD_SIX,
	CARD_SEVEN,
	CARD_EIGHT,
	CARD_NINE,
	CARD_TEN,
	CARD_JACK,
	CARD_QUEEN,
	CARD_KING,
	CARD_KIND_COUNT,
} CardKind;

typedef enum {
	SUIT_CLUB,
	SUIT_DIAMOND,
	SUIT_HEART,
	SUIT_SPADE,
	SUIT_COUNT
} Suit;

typedef struct {
	Suit suit;
	CardKind kind;
	oc_vec2 pos;
	oc_vec2 drag_offset; // offset from mouse to top left corner of card
	oc_vec2 pos_before_drag;
	bool face_up;
	oc_list_elt node;
} Card;

typedef struct {
    float x;
    float y;
    float deltaX;
    float deltaY;
    bool down, was_down;
} MouseInput;

typedef struct {
	oc_surface surface;
	oc_canvas canvas;
	MouseInput mouse_input;

	oc_vec2 frame_size;
	oc_vec2 board_margin;
	u32 card_width, card_height;
	u32 card_margin_x;
	u32 tableau_margin_top;

	Pile stock, waste;
	Pile foundations[4];
	Pile tableau[7];

	Card *card_dragging;
	
	oc_image spritesheet, card_backs[6];
	u32 selected_card_back;
	oc_rect card_sprite_rects[SUIT_COUNT][CARD_KIND_COUNT]; 
	Card cards[SUIT_COUNT*CARD_KIND_COUNT];

} GameState;

GameState game;

void set_sizes_based_on_viewport(u32 width, u32 height) {
    game.frame_size.x = width;
    game.frame_size.y = height;
	game.board_margin.x = 50;
	game.board_margin.y = 25;
	game.card_width = ((f32)width * 0.75f) / 7.0f;
	game.card_height = (f32)game.card_width / CARD_ASPECT;
	game.card_margin_x = (((f32)width * 0.25f) - (2.0f * (f32)game.board_margin.x)) / (ARRAY_COUNT(game.tableau) - 1);
	game.tableau_margin_top = 25;

	game.stock.pos.x = game.board_margin.x;
	game.stock.pos.y = game.board_margin.y;
	game.waste.pos.x = game.stock.pos.x + game.card_width + game.card_margin_x;
	game.waste.pos.y = game.board_margin.y;
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		game.foundations[i].pos.x = game.board_margin.x + ((i + 3) * (game.card_width + game.card_margin_x));
		game.foundations[i].pos.y = game.board_margin.y;
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		game.tableau[i].pos.x = game.board_margin.x + i*(game.card_width + game.card_margin_x);
		game.tableau[i].pos.y = game.board_margin.y + game.card_height + game.tableau_margin_top;
	}
}

static void load_card_images(void) {
	game.card_backs[0] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-00.png"), false);
	game.card_backs[1] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-01.png"), false);
	game.card_backs[2] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-02.png"), false);
	game.card_backs[3] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-03.png"), false);
	game.card_backs[4] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-04.png"), false);
	game.card_backs[5] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-05_small.png"), false);
	game.spritesheet = oc_image_create_from_path(game.surface, OC_STR8("classic_13x4x280x390_compressed.png"), false);

	u32 card_width = 280; 
	u32 card_height = 390;
	u32 rows = 4;
	u32 cols = 13;
	for (i32 j=0; j<rows; ++j)
	for (i32 i=0; i<cols; ++i) {
		game.card_sprite_rects[j][i] = (oc_rect){
			.x = i * card_width,
			.y = j * card_height,
			.w = card_width,
			.h = card_height,
		};
	}
}

static void deal_klondike(Card *cards, i32 num_cards) {
	assert(SUIT_COUNT * CARD_KIND_COUNT == num_cards);

	for (i32 suit=0; suit < SUIT_COUNT; ++suit)
	for (i32 kind=0; kind < CARD_KIND_COUNT; ++kind) {
		cards[suit * CARD_KIND_COUNT + kind] = (Card) {
			.suit = suit,
			.kind = kind,
			.face_up = false,
			.node = NULL,
		};
	}

	// shuffle_deck(cards, num_cards);

	// put all cards in stock
	for (i32 i=0; i<num_cards; ++i) {
		oc_list_push(&game.stock.cards, &cards[i].node);
		cards[i].pos.x = game.stock.pos.x - i;
		cards[i].pos.y = game.stock.pos.y - i;
	}

	// deal to tableau
	for (i32 i=0; i<7; ++i) {
		// deal i cards face down
		i32 face_down_y_offset = 0.125f * game.card_height;
		i32 face_up_y_offset = 0.25f * game.card_height;
		i32 y_offset = 0;
		for (i32 i_face_down=0; i_face_down < i; ++i_face_down) {
			oc_list_elt *node = oc_list_pop(&game.stock.cards);
			Card *card = oc_list_entry(node, Card, node);
			card->face_up = false;
			card->pos.x = game.tableau[i].pos.x;
			card->pos.y = game.tableau[i].pos.y + y_offset;
			y_offset += card->face_up ? face_up_y_offset : face_down_y_offset;
			oc_list_push(&game.tableau[i].cards, node);
		}

		// deal 1 card face up
		// TODO: DELETE LOOP
		for (i32 j=0; j<3; ++j) {
			oc_list_elt *node = oc_list_pop(&game.stock.cards);
			Card *card = oc_list_entry(node, Card, node);
			card->face_up = true;
			card->pos.x = game.tableau[i].pos.x;
			card->pos.y = game.tableau[i].pos.y + y_offset;
			y_offset += card->face_up ? face_up_y_offset : face_down_y_offset;
			oc_list_push(&game.tableau[i].cards, node);
		}
	}

	// TODO: DELETE MEE: put some cards in waste and foundations for testing
	for (i32 j=0; j<3; ++j) {
		oc_list_elt *node = oc_list_pop(&game.stock.cards);
		Card *card = oc_list_entry(node, Card, node);
		card->face_up = true;
		card->pos.x = game.waste.pos.x;
		card->pos.y = game.waste.pos.y;
		oc_list_push(&game.waste.cards, node);

		node = oc_list_pop(&game.stock.cards);
		card = oc_list_entry(node, Card, node);
		card->face_up = true;
		card->pos.x = game.foundations[j].pos.x;
		card->pos.y = game.foundations[j].pos.y;
		oc_list_push(&game.foundations[j].cards, node);
	}
}


ORCA_EXPORT void oc_on_init(void) {
    oc_window_set_title(OC_STR8("Solitaire"));
    game.surface = oc_surface_canvas();
    game.canvas = oc_canvas_create();
	oc_vec2 viewport_size = { 1000, 750 };
	set_sizes_based_on_viewport(viewport_size.x, viewport_size.y);
	oc_window_set_size(viewport_size);

	load_card_images();
	game.selected_card_back = 5;

	deal_klondike(game.cards, ARRAY_COUNT(game.cards));
}

ORCA_EXPORT void oc_on_resize(u32 width, u32 height) {
	set_sizes_based_on_viewport(width, height);
	oc_log_info("width=%lu height=%lu", width, height);
}

//------------------------------------------------------------------------------
// Drawing
//------------------------------------------------------------------------------

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

	// TODO(shaw): draw an icon to indicate clicking to reset stock pile

	oc_list_for_reverse(game.stock.cards, card, Card, node) {
		oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
		oc_image_draw(game.card_backs[game.selected_card_back], dest);
	}
}

static void draw_waste(void) {
	oc_list_for_reverse(game.waste.cards, card, Card, node) {
		if (game.card_dragging == card) break;
		oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
		oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
	}
}

static void draw_foundations(void) {
	u32 border_width = 2;
	oc_set_width(border_width);
	oc_set_color_rgba(0.69, 0.69, 0.69, 0.69); // empty pile outline color
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		oc_rect dest = {
			.x = game.foundations[i].pos.x, 
			.y = game.foundations[i].pos.y,
			.w = game.card_width, 
			.h = game.card_height };

		oc_rounded_rectangle_stroke(
			dest.x + (0.5f * border_width), 
			dest.y + (0.5f * border_width), 
			game.card_width - border_width, 
			game.card_height - border_width,  
			5);

		oc_list_for_reverse(game.foundations[i].cards, card, Card, node) {
			if (game.card_dragging == card) break;
			oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
			oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
		}
	}
}

static void draw_tableau(void) {
	u32 border_width = 2;
	oc_set_width(border_width);
	oc_set_color_rgba(0.42, 0.42, 0.42, 0.69); // empty pile outline color
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {

		oc_rounded_rectangle_stroke(
			game.tableau[i].pos.x + (0.5f * border_width), 
			game.tableau[i].pos.y + (0.5f * border_width), 
			game.card_width - border_width, 
			game.card_height - border_width, 
			5);

		oc_list_for_reverse(game.tableau[i].cards, card, Card, node) {
			if (game.card_dragging == card) break;
			oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
			if (card->face_up) {
				oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
			} else {
				oc_image_draw(game.card_backs[game.selected_card_back], dest);
			}
		}
	}
}

static void draw_dragging(void) {
	if (!game.card_dragging) return;

	for (oc_list_elt *node = &game.card_dragging->node; node; node = oc_list_prev(node)) {
		Card *card = oc_list_entry(node, Card, node);
		oc_rect dest = { card->pos.x, card->pos.y, game.card_width, game.card_height };
		oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
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

static void solitaire_draw(void) {
    oc_canvas_select(game.canvas);
    oc_set_color_rgba(10.0f / 255.0f, 31.0f / 255.0f, 72.0f / 255.0f, 1);
    oc_clear();

	draw_stock();
	draw_waste();
	draw_foundations();
	draw_tableau();

	draw_dragging();

    oc_surface_select(game.surface);
    oc_render(game.canvas);
    oc_surface_present(game.surface);
}

//------------------------------------------------------------------------------
// Game Logic
//------------------------------------------------------------------------------
static bool point_in_card_bounds(f32 x, f32 y, Card *card) {
	if (!card)
		return false;
	return x >= card->pos.x && x < card->pos.x + game.card_width &&
		   y >= card->pos.y && y < card->pos.y + game.card_height;
}


static Card *get_hovered_card(void) {
	f32 mx = game.mouse_input.x;
	f32 my = game.mouse_input.y;
	f32 y_top_row = game.stock.pos.y;
	
	// if mouse is in row with stock and foundations
	if (my >= y_top_row && my < y_top_row + game.card_height) {
		// check stock
		Card *stock_top = oc_list_first_entry(game.stock.cards, Card, node);
		if (point_in_card_bounds(mx, my, stock_top)) {
			return stock_top;
		} 

		// check waste
		Card *waste_top = oc_list_first_entry(game.waste.cards, Card, node);
		if (point_in_card_bounds(mx, my, waste_top)) {
			return waste_top;
		}
				
		// check foundations
		for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
			Card *foundation_top = oc_list_first_entry(game.foundations[i].cards, Card, node);
			if (point_in_card_bounds(mx, my, foundation_top)) {
				return foundation_top;
			}
		}

	} else {
		// check tableau
		// NOTE: ignores face down cards
		for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
			oc_list_for(game.tableau[i].cards, card, Card, node) {
				if (!card->face_up) break;
				if (point_in_card_bounds(mx, my, card)) {
					return card;
				}
			}
		}
	}

	return NULL;
}

// is it legal to drag this card and those on top of it?
static bool can_drag(Card *card) {
	for (oc_list_elt *node = card->node.prev; node; node = node->prev) {
		Card *prev_card = oc_list_entry(node, Card, node);
		if (card->kind - prev_card->kind != 1) {
			return false;
		}
		card = prev_card;
	}
	return card->face_up;
}

static void solitaire_update(void) {
	bool mouse_pressed = game.mouse_input.down && !game.mouse_input.was_down;
	bool mouse_released = !game.mouse_input.down && game.mouse_input.was_down;

	// find card currently being dragged
	if (mouse_pressed) {
		Card *card_dragging = get_hovered_card();
		if (can_drag(card_dragging)) {
			// store pos and drag offset for all cards being dragged together
			for (oc_list_elt *node = &card_dragging->node; node; node = node->prev) {
				Card *card = oc_list_entry(node, Card, node);
				card->pos_before_drag = card->pos;
				card->drag_offset.x = game.mouse_input.x - card->pos.x;
				card->drag_offset.y = game.mouse_input.y - card->pos.y;
			}
			game.card_dragging = card_dragging;
		}

	} else if (mouse_released) {
		if (game.card_dragging) {
			// if it is released on another card, place it on that pile if possible

			// else return the cards to pile they were on previously
			for (oc_list_elt *node = &game.card_dragging->node; node; node = node->prev) {
				Card *card = oc_list_entry(node, Card, node);
				card->pos = card->pos_before_drag;
			}
			game.card_dragging = NULL;
		}
	}

	// move cards being dragged
	if (game.card_dragging) {
		for (oc_list_elt *node = &game.card_dragging->node; node; node = node->prev) {
			Card *card = oc_list_entry(node, Card, node);
			card->pos.x = game.mouse_input.x - card->drag_offset.x;
			card->pos.y = game.mouse_input.y - card->drag_offset.y;
		}
	}

	// if stock clicked, move a card to waste
	// if mouse dragged, move card if possible
	// if mouse released while dragging a card
	    // if it is released on another card, place it on that pile if possible
		// else return the card to pile it was on previously

	// check for win condition

	// TODO: if card is double clicked, move to foundation if possible and there is an open place for it
	
	game.mouse_input.was_down = game.mouse_input.down;
}

ORCA_EXPORT void oc_on_mouse_down(int button) {
    game.mouse_input.down = true;
}

ORCA_EXPORT void oc_on_mouse_up(int button) {
    game.mouse_input.down = false;
}

ORCA_EXPORT void oc_on_mouse_move(float x, float y, float dx, float dy) {
    game.mouse_input.x = x;
    game.mouse_input.y = y;
    game.mouse_input.deltaX = dx;
    game.mouse_input.deltaY = dy;
}

ORCA_EXPORT void oc_on_frame_refresh(void) {
	solitaire_update();
	solitaire_draw();
}


