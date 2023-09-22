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
	bool face_up;
	oc_list_elt node;
} Card;

typedef struct {
	oc_surface surface;
	oc_canvas canvas;

	oc_vec2 frame_size;
	oc_vec2 board_margin;
	u32 card_width, card_height;
	u32 card_margin_x;
	u32 tableau_margin_top;

	Pile stock, waste;
	Pile foundations[4];
	Pile tableau[7];
	
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

	for (i32 i=0; i<num_cards; ++i) {
		oc_list_push(&game.stock.cards, &cards[i].node);
	}

	// deal to tableau
	for (i32 i=0; i<7; ++i) {
		// i cards face down
		for (i32 i_face_down=0; i_face_down < i; ++i_face_down) {
			oc_list_elt *node = oc_list_pop(&game.stock.cards);
			Card *card = oc_list_entry(node, Card, node);
			card->face_up = false;
			oc_list_push(&game.tableau[i].cards, node);
		}

		// 1 card face up
		// TODO: DELETE LOOP
		for (i32 j=0; j<3; ++j) {
		oc_list_elt *node = oc_list_pop(&game.stock.cards);
		Card *card = oc_list_entry(node, Card, node);
		card->face_up = true;
		oc_list_push(&game.tableau[i].cards, node);
		}
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

	i32 card_index = 0;
	oc_list_for_reverse(game.stock.cards, card, Card, node) {
		oc_rect dest = {
			.x = game.stock.pos.x - card_index, 
			.y = game.stock.pos.y - card_index,
			.w = game.card_width, 
			.h = game.card_height };
		oc_image_draw(game.card_backs[game.selected_card_back], dest);
		++card_index;
	}
}

static void draw_waste(void) {

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

		if (i > 0) {
		oc_image_draw_region(game.spritesheet, game.card_sprite_rects[0][0], dest);
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

		i32 face_down_y_offset = 0.125f * game.card_height;
		i32 face_up_y_offset = 0.25f * game.card_height;
		i32 y_offset = 0;
		oc_list_for_reverse(game.tableau[i].cards, card, Card, node) {
			oc_rect dest = {
				.x = game.tableau[i].pos.x, 
				.y = game.tableau[i].pos.y + y_offset,
				.w = game.card_width, 
				.h = game.card_height };

			if (card->face_up) {
				oc_image_draw_region(game.spritesheet, game.card_sprite_rects[card->suit][card->kind], dest);
				y_offset += face_up_y_offset;
			} else {
				oc_image_draw(game.card_backs[game.selected_card_back], dest);
				y_offset += face_down_y_offset;
			}
		}
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

static void draw_board(void) {

	draw_stock();
	draw_waste();
	draw_foundations();
	draw_tableau();
}

ORCA_EXPORT void oc_on_frame_refresh(void) {

	// draw
    oc_canvas_select(game.canvas);
    oc_set_color_rgba(10.0f / 255.0f, 31.0f / 255.0f, 72.0f / 255.0f, 1);
    oc_clear();

	draw_board();

	// render + present
    oc_surface_select(game.surface);
    oc_render(game.canvas);
    oc_surface_present(game.surface);

}


