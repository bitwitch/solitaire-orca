#include <assert.h>
#include <orca.h>

#include "random.c"

#define CARD_ASPECT (560.0f/780.0f)
#define STOCK_OFFSET_BETWEEN_CARDS 0.5
#define ARRAY_COUNT(array) sizeof(array) / sizeof(*(array))

typedef enum {
	PILE_NONE,
	PILE_STOCK,
	PILE_WASTE,
	PILE_FOUNDATION,
	PILE_TABLEAU,
} PileKind;

typedef struct {
	PileKind kind;
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
	Pile *pile;
	oc_vec2 pos;
	oc_vec2 drag_offset; // offset from mouse to top left corner of card
	oc_vec2 pos_before_drag;
	bool face_up;
	oc_list_elt node;
} Card;

typedef struct {
	bool down, was_down;
} DigitalInput;

typedef struct {
    float x;
    float y;
    float deltaX;
    float deltaY;
	DigitalInput left;
} MouseInput;

typedef struct {
	DigitalInput r;
	DigitalInput num1, num2, num3, num4, num5;
} Input;

typedef struct {
	bool win;
	oc_surface surface;
	oc_canvas canvas;
	oc_font font;
	MouseInput mouse_input;
	Input input;

	oc_vec2 frame_size;
	oc_vec2 board_margin;
	u32 card_width, card_height;
	u32 card_margin_x;
	u32 tableau_margin_top;

	Pile stock, waste;
	Pile foundations[4];
	Pile tableau[7];

	Card *card_dragging;
	
	oc_image spritesheet, card_backs[5];
	u32 selected_card_back;
	oc_rect card_sprite_rects[SUIT_COUNT][CARD_KIND_COUNT]; 
	Card cards[SUIT_COUNT*CARD_KIND_COUNT];

} GameState;

GameState game;

static void print_card_info(Card *card);
static char *describe_suit(Suit suit);
static char *describe_card_kind(CardKind kind);

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


static void position_card_on_top_of_pile(Card *card, Pile *pile) {
	switch(pile->kind) {
	case PILE_STOCK: {
		Card *top = oc_list_first_entry(pile->cards, Card, node);
		if (top) {
			card->pos.x = top->pos.x - STOCK_OFFSET_BETWEEN_CARDS;
			card->pos.y = top->pos.y - STOCK_OFFSET_BETWEEN_CARDS;
		} else {
			card->pos = pile->pos;
		}
		break;
	}
	case PILE_WASTE:
	case PILE_FOUNDATION:
		card->pos = pile->pos;
		break;
	case PILE_TABLEAU: {
		Card *top = oc_list_first_entry(pile->cards, Card, node);
		if (top) {
			i32 y_offset = top->face_up ? (0.25f * game.card_height) : (0.125f * game.card_height);
			card->pos.x = top->pos.x;
			card->pos.y = top->pos.y + y_offset;
		} else {
			card->pos = pile->pos;
		}
		break;
	}
	default:
		assert(0);
		break;
	}
}

static void position_all_cards_on_pile(Pile *pile) {
	switch (pile->kind) {
	case PILE_WASTE:
	case PILE_FOUNDATION: {
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->pos = pile->pos;
		}
		break;
	}
	case PILE_STOCK: {
		f32 offset = 0;
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->pos.x = pile->pos.x - offset;
			card->pos.y = pile->pos.y - offset;
			offset += STOCK_OFFSET_BETWEEN_CARDS;
		}
		break;
	}
	case PILE_TABLEAU: {
		i32 y_offset_face_up = 0.25f * game.card_height;
		i32 y_offset_face_down = 0.125f * game.card_height;
		i32 y_offset = 0;
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->pos.x = pile->pos.x;
			card->pos.y = pile->pos.y + y_offset;
			y_offset += card->face_up ? y_offset_face_up : y_offset_face_down;
		}
		break;
	}
	default:
		assert(0);
		break;
	}
}

static Card *pile_pop(Pile *pile) {
	Card *card = oc_list_pop_entry(&pile->cards, Card, node);
	if (card) card->pile = NULL;
	return card;
}

static Card *pile_peek_top(Pile *pile) {
	return oc_list_first_entry(pile->cards, Card, node);
}

static void pile_push(Pile *pile, Card *card) {
	position_card_on_top_of_pile(card, pile);
	card->pile = pile;
	oc_list_push(&pile->cards, &card->node);
}

// this is basically moving a sublist from one list to another
// rather than a single element 
static void pile_transfer(Pile *target_pile, Card *card) {
	assert(card->pile);
	oc_list_elt *node = &card->node;

	oc_list *old_list = &card->pile->cards;
	if (node->next) {
		node->next->prev = NULL;
		old_list->first = node->next;
		node->next = NULL;
	} else {
		old_list->first = NULL;
		old_list->last = NULL;
	}

	oc_list_elt *top = oc_list_begin(target_pile->cards);
	if (top) {
		node->next = top;
		top->prev = node;
	} else {
		target_pile->cards.last = node;
	}

	position_card_on_top_of_pile(card, target_pile);
	card->pile = target_pile;
	target_pile->cards.first = node;

	while (node->prev) {
		node = node->prev;
		Card *current = oc_list_entry(node, Card, node);
		position_card_on_top_of_pile(current, target_pile);
		current->pile = target_pile;
		target_pile->cards.first = node;
	}
}

static void shuffle_deck(Card *cards, i32 num_cards) {
	for (i32 i=num_cards-1; i > 0; --i) {
		i32 j = (i32)rand_range_u32(0, i);
		Card tmp = cards[i];
		cards[i] = cards[j];
		cards[j] = tmp;
	}
}

static void deal_klondike(Card *cards, i32 num_cards) {
	assert(SUIT_COUNT * CARD_KIND_COUNT == num_cards);

	for (i32 suit=0; suit < SUIT_COUNT; ++suit)
	for (i32 kind=0; kind < CARD_KIND_COUNT; ++kind) {
		Card *card = &cards[suit * CARD_KIND_COUNT + kind];
		memset(card, 0, sizeof(*card));
		card->suit = suit;
		card->kind = kind;
	}

	shuffle_deck(cards, num_cards);

	// put all cards in stock
	for (i32 i=0; i<num_cards; ++i) {
		pile_push(&game.stock, &cards[i]);
	}

	// deal to tableau
	for (i32 i=0; i<7; ++i) {
		// deal i cards face down
		i32 face_down_y_offset = 0.125f * game.card_height;
		i32 face_up_y_offset = 0.25f * game.card_height;
		i32 y_offset = 0;
		for (i32 i_face_down=0; i_face_down < i; ++i_face_down) {
			Card *card = pile_peek_top(&game.stock);
			card->face_up = false;
			pile_transfer(&game.tableau[i], card);
		}

		// deal 1 card face up
		Card *card = pile_peek_top(&game.stock);
		card->face_up = true;
		pile_transfer(&game.tableau[i], card);
	}
}

static void game_reset(void) {
	game.win = false;
	game.card_dragging = false;
	memset(&game.mouse_input, 0, sizeof(game.mouse_input));
	memset(&game.input, 0, sizeof(game.input));
	oc_list_init(&game.stock.cards);
	oc_list_init(&game.waste.cards);
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		oc_list_init(&game.foundations[i].cards);
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		oc_list_init(&game.tableau[i].cards);
	}

	deal_klondike(game.cards, ARRAY_COUNT(game.cards));
}

ORCA_EXPORT void oc_on_init(void) {
	// initialize random number generator
	f64 ftime = oc_clock_time(OC_CLOCK_MONOTONIC);
	u64 time = *((u64*)&ftime);
	pcg32_init(time);

    oc_window_set_title(OC_STR8("Solitaire"));
    game.surface = oc_surface_canvas();
    game.canvas = oc_canvas_create();

    oc_unicode_range ranges[5] = {
        OC_UNICODE_BASIC_LATIN,
        OC_UNICODE_C1_CONTROLS_AND_LATIN_1_SUPPLEMENT,
        OC_UNICODE_LATIN_EXTENDED_A,
        OC_UNICODE_LATIN_EXTENDED_B,
        OC_UNICODE_SPECIALS,
    };
	game.font = oc_font_create_from_path(OC_STR8("segoeui.ttf"), 5, ranges);

	oc_vec2 viewport_size = { 1000, 750 };
	set_sizes_based_on_viewport(viewport_size.x, viewport_size.y);
	oc_window_set_size(viewport_size);

	load_card_images();
	game.selected_card_back = 0;

	game.stock.kind = PILE_STOCK;
	game.waste.kind = PILE_WASTE;
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		game.foundations[i].kind = PILE_FOUNDATION;
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		game.tableau[i].kind = PILE_TABLEAU;
	}

	deal_klondike(game.cards, ARRAY_COUNT(game.cards));
}

ORCA_EXPORT void oc_on_resize(u32 width, u32 height) {
	oc_log_info("width=%lu height=%lu", width, height);

	set_sizes_based_on_viewport(width, height);

	position_all_cards_on_pile(&game.stock);
	position_all_cards_on_pile(&game.waste);
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		position_all_cards_on_pile(&game.foundations[i]);
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		position_all_cards_on_pile(&game.tableau[i]);
	}
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
    oc_set_color_rgba(10.0f / 255.0f, 31.0f / 255.0f, 72.0f / 255.0f, 1);
    oc_clear();

	draw_stock();
	draw_waste();
	draw_foundations();
	draw_tableau();

	draw_dragging();

	if (game.win) {
		draw_win_text();
	}

    oc_surface_select(game.surface);
    oc_render(game.canvas);
    oc_surface_present(game.surface);
}

//------------------------------------------------------------------------------
// Game Logic
//------------------------------------------------------------------------------

static inline bool pressed(DigitalInput button) {
	return button.down && !button.was_down;
}

static inline bool released(DigitalInput button) {
	return !button.down && button.was_down;
}

static bool point_in_rect(f32 x, f32 y, oc_rect rect) {
	return x >= rect.x && x < rect.x + rect.w &&
	       y >= rect.y && y < rect.y + rect.h;
}

static bool point_in_card_bounds(f32 x, f32 y, Card *card) {
	if (!card)
		return false;
	return x >= card->pos.x && x < card->pos.x + game.card_width &&
		   y >= card->pos.y && y < card->pos.y + game.card_height;
}

static Pile *get_hovered_pile(void) {
	f32 mx = game.mouse_input.x;
	f32 my = game.mouse_input.y;

	oc_vec2 pos = game.stock.pos;
	oc_rect rect = { pos.x, pos.y, game.card_width, game.card_height };
	if (point_in_rect(mx, my, rect)) {
		return &game.stock;
	} 

	pos = game.waste.pos;
	rect = (oc_rect){ pos.x, pos.y, game.card_width, game.card_height };
	if (point_in_rect(mx, my, rect)) {
		return &game.waste;
	} 

	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		oc_vec2 pos = game.foundations[i].pos;
		rect = (oc_rect){ pos.x, pos.y, game.card_width, game.card_height };
		if (point_in_rect(mx, my, rect)) {
			return &game.foundations[i];
		} 
	}

	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		pos = game.tableau[i].pos;
		rect = (oc_rect){ pos.x, pos.y, game.card_width, game.card_height };
		if (point_in_rect(mx, my, rect)) {
			return &game.tableau[i];
		}
	}

	return NULL;
}

// returns the card the mouse is currently over
// if a card is currently being dragged, this card is ignored and the card
// behind is returned instead
static Card *get_hovered_card(void) {
	f32 mx = game.mouse_input.x;
	f32 my = game.mouse_input.y;
	f32 y_top_row = game.stock.pos.y;
	Card *drag_card = game.card_dragging;
	
	// if mouse is in row with stock and foundations
	if (my >= y_top_row && my < y_top_row + game.card_height) {
		// check stock
		Card *stock_top = oc_list_first_entry(game.stock.cards, Card, node);
		bool top_is_drag_card = stock_top && stock_top == drag_card;
		if (!top_is_drag_card && point_in_card_bounds(mx, my, stock_top)) {
			return stock_top;
		} 

		// check waste
		Card *waste_top = oc_list_first_entry(game.waste.cards, Card, node);
		top_is_drag_card = waste_top && waste_top == drag_card;
		if (!top_is_drag_card && point_in_card_bounds(mx, my, waste_top)) {
			return waste_top;
		}

		// check foundations
		for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
			Card *foundation_top = oc_list_first_entry(game.foundations[i].cards, Card, node);
			if (foundation_top && foundation_top == drag_card) continue;
			if (point_in_card_bounds(mx, my, foundation_top)) {
				return foundation_top;
			}
		}

	} else {
		// check tableau
		for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
			oc_list_for(game.tableau[i].cards, card, Card, node) {
				if (card == drag_card) continue;
				if (point_in_card_bounds(mx, my, card)) {
					return card;
				}
			}
		}
	}

	return NULL;
}

static bool opposite_color_suits(Card *a, Card *b) {
	if (a->suit == SUIT_CLUB || a->suit == SUIT_SPADE) {
		return b->suit == SUIT_DIAMOND || b->suit == SUIT_HEART;
	} else {
		return b->suit == SUIT_CLUB || b->suit == SUIT_SPADE;
	}
}

// is it legal to drag this card and those on top of it?
static bool can_drag(Card *card) {
	for (oc_list_elt *node = card->node.prev; node; node = node->prev) {
		Card *prev_card = oc_list_entry(node, Card, node);
		if (!opposite_color_suits(card, prev_card) || (card->kind - prev_card->kind != 1)) {
			return false;
		}
		card = prev_card;
	}
	return card->face_up;
}

static bool can_drop_empty_pile(Card *card, Pile *pile) {
	bool result = false;
	if (oc_list_empty(pile->cards)) {
		if (pile->kind == PILE_FOUNDATION) {
			result = card->kind == CARD_ACE;
		} else if (pile->kind == PILE_TABLEAU) {
			result = true;
		}
	}
	return result;
}

static bool can_drop(Card *card, Card *target) {
	assert(target);
	assert(target->pile);
	bool result = false;
	Pile *pile = target->pile;
	if (pile->kind == PILE_FOUNDATION) {
		result = (card->suit == target->suit) && 
		         (card->kind == target->kind + 1) &&
				 (card->node.prev == NULL);
	} else if (pile->kind == PILE_TABLEAU) {
		result = opposite_color_suits(card, target) && 
		        (card->kind == target->kind - 1);
	}
	return result;
}

static char *describe_suit(Suit suit) {
	switch (suit) {
		case SUIT_CLUB:    return "club";
		case SUIT_DIAMOND: return "diamond";
		case SUIT_SPADE:   return "spade";
		case SUIT_HEART:   return "heart";
		default:           return "unknown";
	}
}

static char *describe_card_kind(CardKind kind) {
	switch (kind) {
		case CARD_ACE:   return "ace";
		case CARD_TWO:   return "two";
		case CARD_THREE: return "three";
		case CARD_FOUR:  return "four";
		case CARD_FIVE:  return "five";
		case CARD_SIX:   return "six";
		case CARD_SEVEN: return "seven";
		case CARD_EIGHT: return "eight";
		case CARD_NINE:  return "nine";
		case CARD_TEN:   return "ten";
		case CARD_JACK:  return "jack";
		case CARD_QUEEN: return "queen";
		case CARD_KING:  return "king";
		default:         return "unknown";
	}
}

static char *pile_names[] = {
	"stock", "waste", 
	"foundations[0]", "foundations[1]", "foundations[2]", "foundations[3]", 
	"tableau[0]", "tableau[1]", "tableau[2]", "tableau[3]", "tableau[4]", "tableau[5]", "tableau[6]"
};
static char *describe_pile(Pile *pile) {
	if (pile == &game.stock) return pile_names[0];
	if (pile == &game.waste) return pile_names[1];
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		if (pile == &game.foundations[i]) {
			return pile_names[i + 2];
		}
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		if (pile == &game.tableau[i]) {
			return pile_names[i + 6];
		}
	}
	return "unknown";
}

static void print_card_info(Card *card) {
	Card *prev = oc_list_prev_entry(card->pile->cards, card, Card, node);
	Card *next = oc_list_next_entry(card->pile->cards, card, Card, node);
	oc_log_info("\nCard: %s %s\n\tpile = %s\n\tpos = (%f, %f)\n\tface_up = %s\n\tprev = %s %s\n\tnext = %s %s",
		describe_card_kind(card->kind),
		describe_suit(card->suit),
		describe_pile(card->pile),
		card->pos.x, card->pos.y,
		card->face_up ? "true" : "false",
		prev ? describe_card_kind(prev->kind) : "none",
		prev ? describe_suit(prev->suit) : "none",
		next ? describe_card_kind(next->kind) : "none",
		next ? describe_suit(next->suit) : "none");
}

static bool is_game_won(void) {
	bool win = true;
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		Card *top = pile_peek_top(&game.foundations[i]);
		if (!top || top->kind != CARD_KING) {
			win = false;
			break;
		}
	}
	return win;
}

static void end_frame_input(void) {
	game.mouse_input.left.was_down = game.mouse_input.left.down;
	game.input.r.was_down = game.input.r.down;
}

static void solitaire_update(void) {
	if (pressed(game.mouse_input.left)) {
		Card *clicked_card = get_hovered_card();

		if (clicked_card) {
			// start dragging card
			if (can_drag(clicked_card)) {
				// store pos and drag offset for all cards being dragged together
				for (oc_list_elt *node = &clicked_card->node; node; node = node->prev) {
					Card *card = oc_list_entry(node, Card, node);
					card->pos_before_drag = card->pos;
					card->drag_offset.x = game.mouse_input.x - card->pos.x;
					card->drag_offset.y = game.mouse_input.y - card->pos.y;
				}
				game.card_dragging = clicked_card;
			}

			// if stock clicked, move a card to waste
			if (clicked_card == oc_list_first_entry(game.stock.cards, Card, node)) {
				Card *card = pile_peek_top(&game.stock);
				card->face_up = true;
				pile_transfer(&game.waste, card);
			}
		} else {
			// if empty stock clicked, move all waste to stock
			if (oc_list_empty(game.stock.cards)) {
				oc_rect stock_rect = { game.stock.pos.x, game.stock.pos.y, game.card_width, game.card_height };
				if (point_in_rect(game.mouse_input.x, game.mouse_input.y, stock_rect)) {
					Card *card = pile_peek_top(&game.waste);
					while (card) {
						card->face_up = false;
						pile_transfer(&game.stock, card);
						card = pile_peek_top(&game.waste);
					}	
				}
			}
		}

	} else if (released(game.mouse_input.left)) {
		if (game.card_dragging) {
			Card *target = get_hovered_card();
			bool move_success = false;

			if (target) {
				if (can_drop(game.card_dragging, target)) {
					pile_transfer(target->pile, game.card_dragging);
					move_success = true;
				}
			} else {
				Pile *pile = get_hovered_pile();
				if (pile && can_drop_empty_pile(game.card_dragging, pile)) {
					pile_transfer(pile, game.card_dragging);
					move_success = true;
				}
			}

			if (move_success) {
				// if a card at the top of tableau has been revealed turn it over
				for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
					Card *top = pile_peek_top(&game.tableau[i]);
					if (top && !top->face_up) {
						top->face_up = true;
					}
				}

				if (is_game_won()) {
					game.win = true;
					oc_log_info("YOU WIN!");
				}
			} else {
				// return cards to previous position
				for (oc_list_elt *node = &game.card_dragging->node; node; node = node->prev) {
					Card *card = oc_list_entry(node, Card, node);
					card->pos = card->pos_before_drag;
				}
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

	if (pressed(game.input.num1)) {
		game.selected_card_back = 0;
	} else if (pressed(game.input.num2)) {
		game.selected_card_back = 1;
	} else if (pressed(game.input.num3)) {
		game.selected_card_back = 2;
	} else if (pressed(game.input.num4)) {
		game.selected_card_back = 3;
	} else if (pressed(game.input.num5)) {
		game.selected_card_back = 4;
	}

	if (pressed(game.input.r)) {
		game_reset();
	}

	end_frame_input();
}

ORCA_EXPORT void oc_on_key_down(oc_scan_code scan, oc_key_code key) {
	switch (key) {
	case OC_KEY_R: game.input.r.down = true;    break;
	case OC_KEY_1: game.input.num1.down = true; break;
	case OC_KEY_2: game.input.num2.down = true; break;
	case OC_KEY_3: game.input.num3.down = true; break;
	case OC_KEY_4: game.input.num4.down = true; break;
	case OC_KEY_5: game.input.num5.down = true; break;
	default:
		break;
	}

}

ORCA_EXPORT void oc_on_key_up(oc_scan_code scan, oc_key_code key) {
	switch (key) {
	case OC_KEY_R: game.input.r.down = false;    break;
	case OC_KEY_1: game.input.num1.down = false; break;
	case OC_KEY_2: game.input.num2.down = false; break;
	case OC_KEY_3: game.input.num3.down = false; break;
	case OC_KEY_4: game.input.num4.down = false; break;
	case OC_KEY_5: game.input.num5.down = false; break;
	default:
		break;
	}
}

ORCA_EXPORT void oc_on_mouse_down(int button) {
	if (button == OC_MOUSE_LEFT) {
		game.mouse_input.left.down = true;
	}
}

ORCA_EXPORT void oc_on_mouse_up(int button) {
	if (button == OC_MOUSE_LEFT) {
		game.mouse_input.left.down = false;
	}
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


