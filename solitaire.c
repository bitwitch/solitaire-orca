#include <assert.h>
#include <orca.h>
#include <math.h>

#include "random.c"
#include "common.c"
#include "draw.c"

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

static f32 vec2_dist(oc_vec2 v1, oc_vec2 v2) {
	f32 a = v2.x - v1.x;
	f32 b = v2.y - v1.y;
	return sqrtf(a*a + b*b);
}

static inline bool pressed(DigitalInput button) {
	return button.down && !button.was_down;
}

static inline bool released(DigitalInput button) {
	return !button.down && button.was_down;
}

static inline bool point_in_rect(f32 x, f32 y, oc_rect rect) {
	return x >= rect.x && x < rect.x + rect.w &&
	       y >= rect.y && y < rect.y + rect.h;
}

static inline bool rect_in_rect(oc_rect a, oc_rect b) {
	return point_in_rect(a.x, a.y, b)       ||
	       point_in_rect(a.x + a.w, a.y, b) ||
	       point_in_rect(a.x, a.y + a.h, b) ||
	       point_in_rect(a.x + a.w, a.y + a.h, b);
}

static bool point_in_card_bounds(f32 x, f32 y, Card *card) {
	if (!card)
		return false;
	return x >= card->pos.x && x < card->pos.x + game.card_width &&
		   y >= card->pos.y && y < card->pos.y + game.card_height;
}

static void set_sizes_based_on_viewport(u32 width, u32 height) {
    game.frame_size.x = width;
    game.frame_size.y = height;

	game.board_margin.y = 50;
	game.board_margin.x = 50;

	if (game.frame_size.x > 2400) {
		game.board_margin.x = (u32)(0.25f * width);
	} else if (game.frame_size.x > 1900) {
		game.board_margin.x = (u32)(0.21f * width);
	} else if (game.frame_size.x > 1600) {
		game.board_margin.x = (u32)(0.17f * width);
	} else if (game.frame_size.x > 1200) {
		game.board_margin.x = (u32)(0.10f * width);
	}

	f32 usable_width = width - (2.0f * game.board_margin.x);
	f32 pct_card_margin = 0.18;

	game.card_width = ((f32)usable_width * (1.0f - pct_card_margin)) / 7.0f;
	game.card_height = (f32)game.card_width / CARD_ASPECT;
	game.card_margin_x = ((f32)usable_width * pct_card_margin) / (ARRAY_COUNT(game.tableau) - 1);
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

static void load_images(void) {
	game.card_backs[0] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-00.png"), false);
	game.card_backs[1] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-01.png"), false);
	game.card_backs[2] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-02.png"), false);
	game.card_backs[3] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-03.png"), false);
	game.card_backs[4] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-04.png"), false);
	game.card_backs[5] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-05.png"), false);
	game.card_backs[6] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-06.png"), false);
	game.card_backs[7] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-07.png"), false);
	game.card_backs[8] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-08.png"), false);
	game.card_backs[9] = oc_image_create_from_path(game.surface, OC_STR8("Card-Back-09.png"), false);
	game.spritesheet = oc_image_create_from_path(game.surface, OC_STR8("classic_13x4x280x390.png"), false);
	game.reload_icon = oc_image_create_from_path(game.surface, OC_STR8("reload.png"), false);
	game.rules_images[0] = oc_image_create_from_path(game.surface, OC_STR8("klondike_rules_draw_1.png"), false);
	game.rules_images[1] = oc_image_create_from_path(game.surface, OC_STR8("klondike_rules_draw_3.png"), false);

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

static Card *pile_peek_top(Pile *pile);

// positions the second and third cards from the top of the waste pile, and
// returns the new position for the top card
static oc_vec2 position_second_and_third_cards_on_waste(Card *second, Card *third, bool instant) {
	Pile *pile = &game.waste;
	oc_vec2 new_pos = pile->pos;
	if (game.draw_three_mode) {
		i32 x_offset = 0.22f * game.card_width;
		if (second) {
			Card *third = oc_list_next_entry(game.waste.cards, second, Card, node);
			if (third) {
				third->target_pos = pile->pos;
				i32 new_x = third->target_pos.x + x_offset;
				second->target_pos.x = new_x;
				if (instant) {
					third->pos = pile->pos;
					second->pos.x = new_x;
				}
			} else {
				second->target_pos = pile->pos;
				if (instant) {
					second->pos = pile->pos;
				}
			}
			new_pos.x = second->target_pos.x + x_offset;
			new_pos.y = second->target_pos.y;
		}
	}
	return new_pos;
}

static void position_card_on_top_of_pile(Card *card, Pile *pile, bool instant) {
	assert(card);
	assert(pile);
	oc_vec2 new_pos = {0};
	switch(pile->kind) {

	case PILE_STOCK: {
		Card *top = oc_list_first_entry(pile->cards, Card, node);
		if (top) {
			new_pos.x = top->target_pos.x - STOCK_OFFSET_BETWEEN_CARDS;
			new_pos.y = top->target_pos.y - STOCK_OFFSET_BETWEEN_CARDS;
		} else {
			new_pos = pile->pos;
		}
		break;
	}

	case PILE_WASTE: {
		Card *second = pile_peek_top(&game.waste);
		Card *third = second ? oc_list_next_entry(game.waste.cards, second, Card, node) : NULL;
		new_pos = position_second_and_third_cards_on_waste(second, third, instant);
		break;
	}

	case PILE_FOUNDATION: {
		new_pos = pile->pos;
		break;
	}

	case PILE_TABLEAU: {
		Card *top = oc_list_first_entry(pile->cards, Card, node);
		if (top) {
			i32 y_offset = top->face_up ? (0.25f * game.card_height) : (0.125f * game.card_height);
			new_pos.x = top->target_pos.x;
			new_pos.y = top->target_pos.y + y_offset;
		} else {
			new_pos = pile->pos;
		}
		break;
	}

	default:
		assert(0);
		break;
	}

	card->target_pos = new_pos;
	if (instant) {
		card->pos = new_pos;
	} 
}

static void position_all_cards_on_pile(Pile *pile, bool instant) {
	switch (pile->kind) {
	case PILE_FOUNDATION: {
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->target_pos = pile->pos;
			if (instant) {
				card->pos = card->target_pos;
			}
		}
		break;
	}

	case PILE_WASTE: {
		oc_list_for_reverse(pile->cards, card, Card, node) {
			if (game.draw_three_mode && card == oc_list_first_entry(pile->cards, Card, node)) {
				Card *second = oc_list_next_entry(pile->cards, card, Card, node);
				Card *third = second ? oc_list_next_entry(pile->cards, second, Card, node) : NULL;
				oc_vec2 new_pos = position_second_and_third_cards_on_waste(second, third, instant);
				card->target_pos = new_pos;
				if (instant) {
					card->pos = new_pos;
				}
			} else {
				card->target_pos = pile->pos;
				if (instant) {
					card->pos = card->target_pos;
				}
			}
		}
		break;
	}

	case PILE_STOCK: {
		f32 offset = 0;
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->target_pos.x = pile->pos.x - offset;
			card->target_pos.y = pile->pos.y - offset;
			if (instant) {
				card->pos = card->target_pos;
			}
			offset += STOCK_OFFSET_BETWEEN_CARDS;
		}
		break;
	}
	case PILE_TABLEAU: {
		i32 y_offset_face_up = 0.25f * game.card_height;
		i32 y_offset_face_down = 0.125f * game.card_height;
		i32 y_offset = 0;
		oc_list_for_reverse(pile->cards, card, Card, node) {
			card->target_pos.x = pile->pos.x;
			card->target_pos.y = pile->pos.y + y_offset;
			if (instant) {
				card->pos = card->target_pos;
			}
			y_offset += card->face_up ? y_offset_face_up : y_offset_face_down;
		}
		break;
	}
	default:
		assert(0);
		break;
	}
}


static void pile_transfer(Pile *target_pile, Card *card, bool instant);

static void undo_push_pile_transfer(Card *card) {
	assert(game.temp_undo_stack_index < ARRAY_COUNT(game.temp_undo_stack));
	Card *parent = oc_list_next_entry(card->pile->cards, card, Card, node);
	UndoInfo move = {
		.kind = UNDO_PILE_TRANSFER,
		.prev_pile = card->pile,
		.card = card, 
		.was_face_up = card->face_up,
		.parent = parent,
		.was_parent_face_up = parent && parent->face_up,
	};
	game.temp_undo_stack[game.temp_undo_stack_index++] = move;
}

static void undo_push_score_change(i32 score_change) {
	assert(game.temp_undo_stack_index < ARRAY_COUNT(game.temp_undo_stack));
	UndoInfo move = {
		.kind = UNDO_SCORE_CHANGE,
		.score_change = score_change,
	};
	game.temp_undo_stack[game.temp_undo_stack_index++] = move;
}

void update_highscore(i32 score) {
	game.highscore = score;
	snprintf(game.highscore_string, sizeof(game.highscore_string), "High Score: %d", game.highscore);
}

void load_highscore(void) {
	oc_str8 path = OC_STR8("highscore.dat");
	oc_file file = oc_file_open(path, OC_FILE_ACCESS_READ, OC_FILE_OPEN_NONE);
    if(oc_file_last_error(file) != OC_IO_OK) {
        oc_log_error("Could not open file %*.s\n", oc_str8_ip(path));
		update_highscore(0);
		return;
    }
 
	i32 score = 0;
	u64 size = sizeof(i32);
	u64 bytes_read = oc_file_read(file, size, (char*)&score);
	if (bytes_read != size) {
		oc_log_error("Couldn't read highscore data\n");
		score = 0;
	} 
	oc_file_close(file);
	update_highscore(score);
}

void save_highscore(void) {
	oc_str8 path = OC_STR8("highscore.dat");
	oc_file file = oc_file_open(path, OC_FILE_ACCESS_WRITE, OC_FILE_OPEN_NONE);
	if(oc_file_last_error(file) != OC_IO_OK) {
		oc_log_error("Could not open file %*.s\n", oc_str8_ip(path));
		return;
	}
 
	u64 size = sizeof(i32);
	u64 bytes_written = oc_file_write(file, size, (char*)&game.highscore);
	if (bytes_written != size) {
		oc_log_error("Failed to save highscore to disk");
	}

	oc_file_close(file);
}

static void update_score(UpdateScoreParams params) {
	switch (params.kind) {
	case SCORE_RESET:
		game.score = 0;
		break;
	case SCORE_PILE_TRANSFER: {
		assert(params.from_pile);
		assert(params.to_pile);
		PileKind from_kind = params.from_pile->kind;
		PileKind to_kind = params.to_pile->kind;
		i32 score_change = 0;
		if (from_kind == PILE_WASTE && to_kind == PILE_TABLEAU) {
			score_change = 5;
		} else if (from_kind == PILE_WASTE && to_kind == PILE_FOUNDATION) {
			score_change = 10;
		} else if (from_kind == PILE_TABLEAU && to_kind == PILE_FOUNDATION) {
			score_change = 10;
		} else if (from_kind == PILE_FOUNDATION && to_kind == PILE_TABLEAU) {
			score_change = -15;
		}
		undo_push_score_change(score_change);
		game.score += score_change;
		break;
	}
	case SCORE_REVEAL_TABLEAU:
		undo_push_score_change(5);
		game.score += 5;
		break;
	case SCORE_RECYCLE_WASTE:
		if (!game.draw_three_mode) {
			undo_push_score_change(-100);
			game.score -= 100;
		}
		break;
	case SCORE_UNDO:
		game.score -= params.score_change;
		break;
	case SCORE_TIME_BONUS: {
		if (game.timer > 0) {
			i32 bonus = (i32)(700000 / game.timer);
			game.score += bonus;
		}
		break;
	}
	default: 
		assert(0);
		break;
	}
	if (game.score < 0) game.score = 0;
	snprintf(game.score_string, sizeof(game.score_string), "Score: %d", game.score);

	if (game.score > game.highscore) {
		update_highscore(game.score);
	}
}

static void update_score_pile_transfer(Pile *from_pile, Pile *to_pile) {
	UpdateScoreParams params = { 
		.kind = SCORE_PILE_TRANSFER, 
		.from_pile = from_pile,
		.to_pile = to_pile};
	update_score(params);
}

static Card *pile_pop(Pile *pile) {
	Card *card = oc_list_pop_entry(&pile->cards, Card, node);
	if (card) card->pile = NULL;
	return card;
}

static Card *pile_peek_top(Pile *pile) {
	return oc_list_first_entry(pile->cards, Card, node);
}

static void pile_push(Pile *pile, Card *card, bool instant) {
	position_card_on_top_of_pile(card, pile, instant);
	card->pile = pile;
	oc_list_push(&pile->cards, &card->node);
}

// this is basically moving a sublist from one list to another
// rather than a single element 
static void pile_transfer(Pile *target_pile, Card *card, bool instant) {
	assert(card->pile);
	Pile *old_pile = card->pile;
	oc_list_elt *node = &card->node;

	oc_list *old_list = &old_pile->cards;
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

	position_card_on_top_of_pile(card, target_pile, instant);
	card->pile = target_pile;
	target_pile->cards.first = node;

	while (node->prev) {
		node = node->prev;
		Card *current = oc_list_entry(node, Card, node);
		position_card_on_top_of_pile(current, target_pile, instant);
		current->pile = target_pile;
		target_pile->cards.first = node;
	}

	if (old_pile->kind == PILE_WASTE) {
		position_all_cards_on_pile(&game.waste, instant);
	}
}

static void update_moves_string(void) {
	i32 total_moves = game.move_count + game.undo_count;
	assert(total_moves <= 9999);
	snprintf(game.moves_string, sizeof(game.moves_string), "Moves: %d", total_moves);
}



static void undo_commit(void) {
	if (game.temp_undo_stack_index > 0) {
		assert(game.undo_stack_index + game.temp_undo_stack_index + 1 < ARRAY_COUNT(game.undo_stack));
		game.undo_stack[game.undo_stack_index++] = (UndoInfo){.kind = UNDO_COMMIT_MARKER};
		for (i32 i=0; i<game.temp_undo_stack_index; ++i) {
			UndoInfo move = game.temp_undo_stack[i];
			game.undo_stack[game.undo_stack_index++] = move;
		}
		game.temp_undo_stack_index = 0;
	}
}

static void commit_move(void) {
	if (game.temp_undo_stack_index > 0) {
		++game.move_count;
		update_moves_string();
	}
	undo_commit();
}

static void undo_move(void) {
	if (game.undo_stack_index > 0) {
		bool cleanup_waste = false;
		UndoInfo undo = game.undo_stack[--game.undo_stack_index];
		while (undo.kind != UNDO_COMMIT_MARKER) {
			assert(game.undo_stack_index > 0);
			switch (undo.kind) {
			case UNDO_PILE_TRANSFER: {
				if (undo.card->pile->kind == PILE_WASTE) {
				 cleanup_waste = true;
				}
				if (undo.parent) {
				 undo.parent->face_up = undo.was_parent_face_up;
				}
				pile_transfer(undo.prev_pile, undo.card, true);
				undo.card->face_up = undo.was_face_up;
				break;
			}
			case UNDO_SCORE_CHANGE: {
				UpdateScoreParams params = { .kind = SCORE_UNDO, .score_change = undo.score_change };
				update_score(params);
				break;
			}
			default:
				assert(0);
				break;
			}
			undo = game.undo_stack[--game.undo_stack_index];
		}
		
		if (cleanup_waste) {
			position_all_cards_on_pile(&game.waste, false);
		}

		// score penalty for undo
		UpdateScoreParams params = { .kind = SCORE_UNDO, .score_change = 15 };
		update_score(params);

		++game.undo_count;
		update_moves_string();
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

static void test_deal_for_autocomplete(Card *cards, i32 num_cards) {
	assert(SUIT_COUNT * CARD_KIND_COUNT == num_cards);
	i32 index = 0;
	
	Suit suit_even = SUIT_DIAMOND;
	Suit suit_odd = SUIT_CLUB;
	for (i32 kind=CARD_KING; kind >= CARD_ACE; --kind) {
		Card *card = &cards[index++];
		memset(card, 0, sizeof(*card));
		card->face_up = true;
		card->suit = (kind % 2) == 0 ? suit_even : suit_odd;
		card->kind = kind;
		pile_push(&game.tableau[0], card, true);
	}

	suit_even = SUIT_CLUB;
	suit_odd = SUIT_DIAMOND;
	for (i32 kind=CARD_KING; kind >= CARD_ACE; --kind) {
		Card *card = &cards[index++];
		memset(card, 0, sizeof(*card));
		card->face_up = true;
		card->suit = (kind % 2) == 0 ? suit_even : suit_odd;
		card->kind = kind;
		pile_push(&game.tableau[1], card, true);
	}

	suit_even = SUIT_HEART;
	suit_odd = SUIT_SPADE;
	for (i32 kind=CARD_KING; kind >= CARD_ACE; --kind) {
		Card *card = &cards[index++];
		memset(card, 0, sizeof(*card));
		card->face_up = true;
		card->suit = (kind % 2) == 0 ? suit_even : suit_odd;
		card->kind = kind;
		pile_push(&game.tableau[2], card, true);
	}

	suit_even = SUIT_SPADE;
	suit_odd = SUIT_HEART;
	for (i32 kind=CARD_KING; kind >= CARD_ACE; --kind) {
		Card *card = &cards[index++];
		memset(card, 0, sizeof(*card));
		card->face_up = true;
		card->suit = (kind % 2) == 0 ? suit_even : suit_odd;
		card->kind = kind;
		pile_push(&game.tableau[3], card, true);
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
		pile_push(&game.stock, &cards[i], true);
	}

	game.state = STATE_DEALING;
}

static void update_timer_string(f64 seconds_elapsed_f64) {
	u64 seconds_elapsed = (u64)seconds_elapsed_f64;
	u64 seconds = seconds_elapsed % 60;
	u64 minutes_elapsed = seconds_elapsed / 60;
	u64 minutes = minutes_elapsed % 60;
	u64 hours = minutes_elapsed / 60;
	snprintf(game.timer_string, sizeof(game.timer_string), "%02llu:%02llu:%02llu", hours, minutes, seconds);
}

static void game_reset(void) {
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

	game.timer = 0;
	update_timer_string(game.timer);

	game.win_foundation_index = 0;
	game.win_moving_card = NULL;
	game.win_card_path_index = 0;

	game.undo_stack_index = 0;
	game.move_count = 0;
	game.undo_count = 0;
	update_moves_string();

	UpdateScoreParams params = { .kind = SCORE_RESET };
	update_score(params);

	game.deal_countdown = 0;
	game.deal_tableau_index = 0;
	game.deal_tableau_remaining = ARRAY_COUNT(game.tableau);
	game.deal_cards_remaining = 28; // 7+6+5+4+3+2+1

	deal_klondike(game.cards, ARRAY_COUNT(game.cards));
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
	bool card_in_motion = card->pos.x != card->target_pos.x || card->pos.y != card->target_pos.y;
	if (card_in_motion) {
		return false;
	}
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
			if (game.draw_three_mode) {
				result = card->kind == CARD_KING;
			} else {
				result = true;
			}
		}
	}
	return result;
}

// is it legal to drop card on target
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

// checks if card is currently colliding with an empty pile or the top card on
// the pile and returns true if it is legal to drop card there, otherwise false
static bool can_drop_card_on_pile(Pile *pile, Card *card) {
	oc_rect card_rect = {
		.x = card->pos.x, 
		.y = card->pos.y,
		.w = game.card_width,
		.h = game.card_height 
	};

	// check empty pile
	if (oc_list_empty(pile->cards)) {
		oc_rect pile_rect = {
			.x = pile->pos.x, 
			.y = pile->pos.y,
			.w = game.card_width,
			.h = game.card_height };
		if (rect_in_rect(card_rect, pile_rect) &&
		    can_drop_empty_pile(card, pile))
		{
			return true;
		}

	// check top card
	} else {
		Card *top = oc_list_first_entry(pile->cards, Card, node);
		oc_rect top_rect = {
			.x = top->pos.x, 
			.y = top->pos.y,
			.w = game.card_width,
			.h = game.card_height 
		};
		if (top != card && 
			rect_in_rect(card_rect, top_rect) &&
		    can_drop(card, top))
		{
			return true;
		}
	}

	return false;
}

static bool maybe_drop_dragged_card(void) {
	Card *drag_card = game.card_dragging;

	// check foundations
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		Pile *pile = &game.foundations[i]; 
		if (can_drop_card_on_pile(pile, drag_card)) {
			update_score_pile_transfer(drag_card->pile, pile);
			undo_push_pile_transfer(drag_card);
			pile_transfer(pile, drag_card, false);
			return true;
		}
	}

	// check tableau
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		Pile *pile = &game.tableau[i];
		if (can_drop_card_on_pile(pile, drag_card)) {
			update_score_pile_transfer(drag_card->pile, pile);
			undo_push_pile_transfer(drag_card);
			pile_transfer(pile, drag_card, false);
			return true;
		}
	}

	return false;
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

static bool is_tableau_empty(void) {
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		if (!oc_list_empty(game.tableau[i].cards)) {
			return false;
		}
	}
	return true;
}

static bool is_autocomplete_possible(void) {
	if (!oc_list_empty(game.stock.cards) || !oc_list_empty(game.waste.cards)) {
		return false;
	}

	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		Pile *pile = &game.tableau[i];
		oc_list_for_reverse(pile->cards, card, Card, node) {
			if (!card->face_up) {
				return false;
			}	
			Card *prev = oc_list_prev_entry(pile->cards, card, Card, node);
			if (prev && prev->kind != card->kind - 1) {
				return false;
			}
		}
	}

	return true;
}

static bool auto_transfer_card_to_foundation(Card *card) {
	if (card->pile->kind == PILE_FOUNDATION || card->pile->kind == PILE_STOCK || card->node.prev != NULL) {
		return false;
	}

	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		bool auto_transfer = false;
		Card *top = pile_peek_top(&game.foundations[i]);
		if (top) {
			if ((top->suit == card->suit) && (top->kind == card->kind - 1)) {
				auto_transfer = true;
			}
		} else if (card->kind == CARD_ACE) {
			auto_transfer = true;
		}

		if (auto_transfer) {
			update_score_pile_transfer(card->pile, &game.foundations[i]);
			undo_push_pile_transfer(card);
			pile_transfer(&game.foundations[i], card, false);
			return true;
		}
	}

	return false;
}

// if a card at the top of tableau has been revealed turn it over
static void reveal_tableau_card(void) {
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		Card *top = pile_peek_top(&game.tableau[i]);
		if (top && !top->face_up) {
			top->face_up = true;
			UpdateScoreParams params = { .kind = SCORE_REVEAL_TABLEAU };
			update_score(params);
		}
	}
}

static bool step_cards_towards_target(f32 rate) {
	bool any_card_moved = false;
	for (i32 i=0; i<ARRAY_COUNT(game.cards); ++i) {
		Card *card = &game.cards[i];
		if (card->pos.x != card->target_pos.x || card->pos.y != card->target_pos.y) {
			if (vec2_dist(card->pos, card->target_pos) < 1) {
				card->pos = card->target_pos;
			} else {
				card->pos.x += (card->target_pos.x - card->pos.x) * rate * game.dt;
				card->pos.y += (card->target_pos.y - card->pos.y) * rate * game.dt;
				any_card_moved = true;
			}
		}
	}
	return any_card_moved;
}

static inline void append_win_card_path(Card *card) {
	if (game.win_card_path_index < ARRAY_COUNT(game.win_card_path)) {
		game.win_card_path[game.win_card_path_index++] = (CardPath){
			.suit = card->suit,	
			.kind = card->kind,	
			.pos = card->pos,
		};
	} else {
		oc_log_error("too many card positions in win_card_path: %d\n", game.win_card_path_index);
		game.win_card_path_index = 0;
	}
}

static void solitaire_update_win(void) {
	Card *card = game.win_moving_card;
	bool launch_next_card = !card 
		|| (card->pos.x + game.card_width < 0)
		|| (card->pos.x > game.frame_size.x);

	if (launch_next_card) {
		card = pile_pop(&game.foundations[game.win_foundation_index]);
		game.win_foundation_index = (game.win_foundation_index + 1) % ARRAY_COUNT(game.foundations);
		if (card) {
			card->vel.y = rand_f32() * 300.0f - 400.0f;
			f32 rand_val = rand_f32();
			f32 dir = rand_val > 0.5f ? -1.0f : 1.0f;
			card->vel.x = (rand_val * 420.0f + 45.0f) * dir;
		}
		game.win_moving_card = card;
	}

	// tick launched card
	if (card) {
		card->vel.y += GRAVITY * game.dt;
		card->pos.x += card->vel.x * game.dt;
		card->pos.y += card->vel.y * game.dt;
		if (card->pos.y + game.card_height >= game.frame_size.y) {
			card->pos.y = game.frame_size.y - game.card_height;
			if (card->vel.y > 0) {
				card->vel.y *= -0.88f;
			} 
		}
		append_win_card_path(card);
	}
}

static void solitaire_update_autocomplete(void) {
	bool tableau_empty = is_tableau_empty();

	if (!tableau_empty && game.deal_countdown <= 0) {
		bool card_transferred = false;
		for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
			if (card_transferred) break;
			Card *tableau_top = pile_peek_top(&game.tableau[i]);
			if (!tableau_top) continue;
			for (i32 j=0; j<ARRAY_COUNT(game.foundations); ++j) {
				if (card_transferred) break;
				Pile *foundation_pile = &game.foundations[j];
				Card *foundation_top = pile_peek_top(foundation_pile);
				if (foundation_top) {
					if (tableau_top->suit == foundation_top->suit &&
						tableau_top->kind == foundation_top->kind + 1)
					{
						update_score_pile_transfer(tableau_top->pile, foundation_pile);
						pile_transfer(foundation_pile, tableau_top, false);
						card_transferred = true;
					}
				} else if (tableau_top->kind == CARD_ACE){
					update_score_pile_transfer(tableau_top->pile, foundation_pile);
					pile_transfer(foundation_pile, tableau_top, false);
					card_transferred = true;
				}
			}
		}
		assert(card_transferred);
		game.deal_countdown += game.deal_delay;
	} else {
		game.deal_countdown -= game.dt;
	}

	bool any_card_moved = step_cards_towards_target(game.deal_speed);

	if (tableau_empty && !any_card_moved) {
		UpdateScoreParams params = { .kind = SCORE_TIME_BONUS };
		update_score(params);
		save_highscore();
		game.state = STATE_WIN;
	}
}

static void solitaire_update_dealing(void) {
	if (game.deal_cards_remaining > 0 && game.deal_countdown <= 0) {
		Card *card = pile_pop(&game.stock);
		--game.deal_cards_remaining;
		i32 i = game.deal_tableau_index;
		i32 remaining = game.deal_tableau_remaining;
		i32 count = ARRAY_COUNT(game.tableau);

		pile_push(&game.tableau[i], card, false);
		if (i == count - remaining) {
			card->face_up = true;
		}

		if (i == count - 1) {
			--game.deal_tableau_remaining;
			game.deal_tableau_index = count - game.deal_tableau_remaining;
		} else {
			++game.deal_tableau_index;
		}
		game.deal_countdown += game.deal_delay;
	} else {
		game.deal_countdown -= game.dt;
	}

	bool any_card_moved = step_cards_towards_target(game.deal_speed);
	
	if (game.deal_cards_remaining == 0 && !any_card_moved) {
		game.state = STATE_PLAY;
	}
}

static void solitaire_update_show_rules(void) {
	if (pressed(game.mouse_input.left)) {
		game.state = game.restore_state;
	}
}

static void solitaire_update_select_card_back(void) {
	// NOTE(shaw): the ui handles transition out of STATE_SELECT_CARD_BACK

	if (!game.menu_card_backs_draw_box) {
		return;
	}
	oc_rect draw_box = game.menu_card_backs_draw_box->rect;
	
	if (pressed(game.mouse_input.left)) {
		i32 count_first_row = ARRAY_COUNT(game.card_backs) / 2;

		for (i32 i=0; i<ARRAY_COUNT(game.card_backs); ++i) {
			f32 x = 0, y = 0;
			if (i < count_first_row) {
				x = draw_box.x + (i * (game.card_width + game.card_margin_x));
				y = draw_box.y;
			} else {
				x = draw_box.x + ((i - count_first_row) * (game.card_width + game.card_margin_x));
				y = draw_box.y + game.card_height + game.card_margin_x;
			}
			oc_rect hitbox = { x, y, game.card_width, game.card_height };

			if (point_in_rect(game.mouse_input.x, game.mouse_input.y, hitbox)) {
				game.selected_card_back = i;
				break;
			}
		}
	}
}

static void solitaire_update_play(void) {
	step_cards_towards_target(game.card_animate_speed);

	// freeze user input dealing with gameplay while menu is open
	if (game.menu_opened) return;

	Card *hovered_card = get_hovered_card();

	if (pressed(game.mouse_input.left)) {
		if (hovered_card) {
			// start dragging card
			if (can_drag(hovered_card)) {
				// store pos and drag offset for all cards being dragged together
				for (oc_list_elt *node = &hovered_card->node; node; node = node->prev) {
					Card *card = oc_list_entry(node, Card, node);
					card->pos_before_drag = card->pos;
					card->drag_offset.x = game.mouse_input.x - card->pos.x;
					card->drag_offset.y = game.mouse_input.y - card->pos.y;
				}
				game.card_dragging = hovered_card;
			}

			// if stock clicked, move cards to waste
			if (hovered_card == oc_list_first_entry(game.stock.cards, Card, node)) {
				i32 cards_to_transfer = game.draw_three_mode ? 3 : 1;
				for (i32 i=0; i<cards_to_transfer; ++i) {
					Card *card = pile_peek_top(&game.stock);
					if (card) {
						undo_push_pile_transfer(card);
						card->face_up = true;
						pile_transfer(&game.waste, card, false);
					}
				}
			}

		} else {
			// if empty stock clicked, move all waste to stock
			if (oc_list_empty(game.stock.cards)) {
				oc_rect stock_rect = { game.stock.pos.x, game.stock.pos.y, game.card_width, game.card_height };
				if (point_in_rect(game.mouse_input.x, game.mouse_input.y, stock_rect)) {
					Card *card = pile_peek_top(&game.waste);
					if (card) {
						UpdateScoreParams params = { .kind = SCORE_RECYCLE_WASTE };
						update_score(params);
					}
					while (card) {
						undo_push_pile_transfer(card);
						card->face_up = false;
						pile_transfer(&game.stock, card, false);
						card = pile_peek_top(&game.waste);
					}	
				}
			}
		}

	} else if (released(game.mouse_input.left)) {
		if (game.card_dragging) {
			bool move_success = false; // default to false
			f32 drag_dist = vec2_dist(game.card_dragging->pos, game.card_dragging->pos_before_drag);
			bool is_card_clicked = drag_dist <= MAX_DIST_CONSIDERED_CLICK;

			if (is_card_clicked) {
				move_success = auto_transfer_card_to_foundation(game.card_dragging);
			} else {
				move_success = maybe_drop_dragged_card();
			}

			if (move_success) {
				reveal_tableau_card();

				if (is_autocomplete_possible()) {
					oc_log_info("autocompleting");
					game.deal_countdown = game.deal_delay;
					game.state = STATE_AUTOCOMPLETE;
				}

				if (is_game_won()) {
					UpdateScoreParams params = { .kind = SCORE_TIME_BONUS };
					update_score(params);
					save_highscore();
					game.state = STATE_WIN;
				}
			} else {
				// return cards to previous position
				for (oc_list_elt *node = &game.card_dragging->node; node; node = node->prev) {
					Card *card = oc_list_entry(node, Card, node);
					card->target_pos = card->pos_before_drag;
				}
			}
	
			game.card_dragging = NULL;
		}

	} else if (pressed(game.mouse_input.right)) {
		game.mouse_pos_on_mouse_right_down = (oc_vec2){ game.mouse_input.x, game.mouse_input.y };

	} else if (released(game.mouse_input.right)) {
		if (hovered_card) {
			f32 d = vec2_dist(game.mouse_pos_on_mouse_right_down, (oc_vec2){game.mouse_input.x, game.mouse_input.y});
			bool is_card_right_clicked = d <= MAX_DIST_CONSIDERED_CLICK;
			if (is_card_right_clicked) {
				bool did_transfer = auto_transfer_card_to_foundation(hovered_card);
				if (did_transfer) {
					reveal_tableau_card();
				}
			}
		}
	} else if (pressed(game.input.u)) {
		undo_move();
	}

	// move cards being dragged
	if (game.card_dragging) {
		for (oc_list_elt *node = &game.card_dragging->node; node; node = node->prev) {
			Card *card = oc_list_entry(node, Card, node);
			card->target_pos.x = game.mouse_input.x - card->drag_offset.x;
			card->target_pos.y = game.mouse_input.y - card->drag_offset.y;
			// NOTE(shaw): it is important to set both pos and target_pos here
			// so that card drops are accurate. 
			card->pos = card->target_pos; 
		}
	}

	commit_move();
}


static void end_frame_input(void) {
	game.mouse_input.left.was_down = game.mouse_input.left.down;
	game.mouse_input.right.was_down = game.mouse_input.right.down;
	game.input.r.was_down = game.input.r.down;
	game.input.u.was_down = game.input.u.down;
	game.input.num1.was_down = game.input.num1.down;
	game.input.num2.was_down = game.input.num2.down;
	game.input.num3.was_down = game.input.num3.down;
	game.input.num4.was_down = game.input.num4.down;
	game.input.num5.was_down = game.input.num5.down;
	game.input.num6.was_down = game.input.num6.down;
	game.input.num7.was_down = game.input.num7.down;
	game.input.num8.was_down = game.input.num8.down;
	game.input.num9.was_down = game.input.num9.down;
	game.input.num0.was_down = game.input.num0.down;
}

static void solitaire_update(void) {
	if (game.state == STATE_PLAY || game.state == STATE_AUTOCOMPLETE) {
		game.timer += game.dt;
		update_timer_string(game.timer);
	}

	switch (game.state) {
	case STATE_DEALING:
		solitaire_update_dealing();
		break;
	case STATE_PLAY:
		solitaire_update_play();
		break;
	case STATE_SHOW_RULES:
		solitaire_update_show_rules();
		break;
	case STATE_SELECT_CARD_BACK:
		solitaire_update_select_card_back();
		break;
	case STATE_AUTOCOMPLETE:
		solitaire_update_autocomplete();
		break;
	case STATE_WIN:
		solitaire_update_win();
		break;
	default: 
		assert(0);
		break;
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
	} else if (pressed(game.input.num6)) {
		game.selected_card_back = 5;
	} else if (pressed(game.input.num7)) {
		game.selected_card_back = 6;
	} else if (pressed(game.input.num8)) {
		game.selected_card_back = 7;
	} else if (pressed(game.input.num9)) {
		game.selected_card_back = 8;
	} else if (pressed(game.input.num0)) {
		game.selected_card_back = 9;
	}

	if (pressed(game.input.r)) {
		game_reset();
	}

	end_frame_input();
}

static void set_restore_state(void) {
	if (game.state != STATE_SHOW_RULES && game.state != STATE_SELECT_CARD_BACK) {
		game.restore_state = game.state;
	}
}

static void do_card_back_menu(void) {
	oc_ui_panel("main panel", OC_UI_FLAG_NONE)
	{
		oc_ui_style_next(&(oc_ui_style){ 
				.size.width = { OC_UI_SIZE_PARENT, 1 },
				.size.height = { OC_UI_SIZE_PARENT, 1, 1 },
				.layout.axis = OC_UI_AXIS_Y,
				.layout.align.x = OC_UI_ALIGN_CENTER,
				.layout.align.y = OC_UI_ALIGN_CENTER,
				.layout.margin.x = 16,
				.layout.margin.y = 16,
				.layout.spacing = 16 },
				OC_UI_STYLE_SIZE
				| OC_UI_STYLE_LAYOUT_AXIS
				| OC_UI_STYLE_LAYOUT_ALIGN_X
				| OC_UI_STYLE_LAYOUT_ALIGN_Y
				| OC_UI_STYLE_LAYOUT_MARGINS
				| OC_UI_STYLE_LAYOUT_SPACING);

		oc_ui_container("card backs", OC_UI_FLAG_NONE)
		{
			oc_ui_style_next(&(oc_ui_style){ 
					.size.width = { OC_UI_SIZE_CHILDREN },
					.size.height = { OC_UI_SIZE_CHILDREN },
					.layout.axis = OC_UI_AXIS_Y,
					.layout.margin.x = game.menu_card_backs_margin,
					.layout.margin.y = game.menu_card_backs_margin,
					.layout.spacing = 24,
					.bgColor = game.ui.theme->bg1,
					.borderColor = game.ui.theme->border,
					.borderSize = 1,
					.roundness = game.ui.theme->roundnessSmall },
					OC_UI_STYLE_SIZE
					| OC_UI_STYLE_LAYOUT_AXIS
					| OC_UI_STYLE_LAYOUT_MARGINS
					| OC_UI_STYLE_LAYOUT_SPACING
					| OC_UI_STYLE_BG_COLOR
					| OC_UI_STYLE_BORDER_COLOR
					| OC_UI_STYLE_BORDER_SIZE
					| OC_UI_STYLE_ROUNDNESS);

			oc_ui_box_begin("Select Card Back", OC_UI_FLAG_DRAW_BACKGROUND | OC_UI_FLAG_DRAW_BORDER);

			oc_ui_style_next(&(oc_ui_style){ 
					.size.width = { OC_UI_SIZE_PARENT, 1 },
					.layout.align.x = OC_UI_ALIGN_CENTER },
					OC_UI_STYLE_SIZE_WIDTH | OC_UI_STYLE_LAYOUT_ALIGN_X);
			oc_ui_container("header", OC_UI_FLAG_NONE)
			{
				oc_ui_style_next(&(oc_ui_style){ .fontSize = 18 },
						OC_UI_STYLE_FONT_SIZE);
				oc_ui_label("Select Card Back");
			}

			oc_ui_style_next(&(oc_ui_style){ 
					.size.width = { OC_UI_SIZE_PARENT, 1 },
					.size.height = { OC_UI_SIZE_PARENT, 1, 1 },
					.layout.align.x = OC_UI_ALIGN_CENTER,
					.layout.align.y = OC_UI_ALIGN_CENTER,
					.layout.spacing = 24 },
					OC_UI_STYLE_SIZE
					| OC_UI_STYLE_LAYOUT_ALIGN_X
					| OC_UI_STYLE_LAYOUT_ALIGN_Y
					| OC_UI_STYLE_LAYOUT_SPACING);
			oc_ui_box_begin("contents", OC_UI_FLAG_NONE);

			// box for drawing card backs into
			i32 cards_per_row = ARRAY_COUNT(game.card_backs) - (ARRAY_COUNT(game.card_backs) / 2);
			f32 row_width = (cards_per_row * game.card_width) + ((cards_per_row - 1) * game.card_margin_x);
			f32 total_height = (2 * game.card_height) + game.card_margin_x;
			oc_ui_style_next(&(oc_ui_style){ 
					.size.width = { OC_UI_SIZE_PIXELS, row_width},
					.size.height = { OC_UI_SIZE_PIXELS, total_height } },
					OC_UI_STYLE_SIZE);
			oc_ui_box_begin("space to draw cards", OC_UI_FLAG_NONE);

			game.menu_card_backs_draw_box = oc_ui_box_top();
			// draw code will draw card backs here and update code will handle selection
			// since orca doesn't seem to have image buttons

			oc_ui_box_end(); // space to draw cards

			oc_ui_style_next(&(oc_ui_style){ .color = game.ui.theme->white }, OC_UI_STYLE_COLOR);
			if(oc_ui_button("OK").clicked) {
				game.state = game.restore_state;
			}

			oc_ui_box_end(); // contents
			oc_ui_box_end(); // card backs
		}
	}
}

static void solitaire_menu(void) {
	oc_ui_box *menu = NULL;

	oc_ui_style style = { .font = game.font, .bgColor = game.menu_bg_color };
	oc_ui_style_mask style_mask = OC_UI_STYLE_FONT | OC_UI_STYLE_BG_COLOR;
	oc_ui_frame(game.frame_size, &style, style_mask) 
	{
		oc_ui_menu_bar("menu_bar") 
		{
			// Game Dropdown Menu
			oc_ui_style_next(&(oc_ui_style){ 
					.size.width = { .kind = OC_UI_SIZE_PARENT, .value = 0.3333, .relax = 1 },
					.size.height = { .kind = OC_UI_SIZE_PARENT, .value = 1 },
					.layout.axis = OC_UI_AXIS_X,
					.layout.align.x = OC_UI_ALIGN_START,
					.layout.align.y = OC_UI_ALIGN_CENTER },
					OC_UI_STYLE_SIZE
					| OC_UI_STYLE_LAYOUT_AXIS
					| OC_UI_STYLE_LAYOUT_ALIGN_X
					| OC_UI_STYLE_LAYOUT_ALIGN_Y);
			oc_ui_menu_begin("Game");
			{
				f32 button_width = 185;
				menu = oc_ui_box_top();

				if (oc_ui_menu_button_fixed_width("New Game", button_width).pressed) {
					game_reset();
				}

				if (oc_ui_menu_button_fixed_width("Undo", button_width).pressed) {
					if (game.state == STATE_PLAY) {
						undo_move();
					}
				}

				{ // game mode buttons: draw 1 or 3
					const char *game_mode_text = game.draw_three_mode 
						? "Switch Game Mode: Turn 1"
						: "Switch Game Mode: Turn 3";
					if (oc_ui_menu_button_fixed_width(game_mode_text, button_width).pressed) {
						game.draw_three_mode = !game.draw_three_mode;
						game_reset();
					}
				}
				
				if (oc_ui_menu_button_fixed_width("How to Play", button_width).pressed) {
					set_restore_state();
					game.state = STATE_SHOW_RULES;
					game.mouse_input.left.down = false;
				}
				if (oc_ui_menu_button_fixed_width("Select Card Back", button_width).pressed) {
					set_restore_state();
					game.state = STATE_SELECT_CARD_BACK;
					game.mouse_input.left.down = false;
				}
				oc_ui_menu_end();
			}

			// High Score
			oc_ui_style_next(&(oc_ui_style){ 
				.size.width = { .kind = OC_UI_SIZE_PARENT, .value = 0.3333, .relax = 1 },
				.size.height = { .kind = OC_UI_SIZE_PARENT, .value = 1 },
				.layout.axis = OC_UI_AXIS_X,
				.layout.align.x = OC_UI_ALIGN_CENTER,
				.layout.align.y = OC_UI_ALIGN_CENTER },
				OC_UI_STYLE_SIZE
				| OC_UI_STYLE_LAYOUT_AXIS
				| OC_UI_STYLE_LAYOUT_ALIGN_X
				| OC_UI_STYLE_LAYOUT_ALIGN_Y);
			oc_ui_container("menu bar middle", OC_UI_FLAG_NONE)
			{
				oc_ui_label(game.highscore_string);
			}

			// Score and Timer
			oc_ui_style_next(&(oc_ui_style){ 
				.size.width  = { .kind = OC_UI_SIZE_PARENT, .value = 0.3333, .relax = 1 },
				.size.height = { .kind = OC_UI_SIZE_PARENT, .value = 1 },
				.layout.axis = OC_UI_AXIS_X,
				.layout.align.x = OC_UI_ALIGN_END,
				.layout.align.y = OC_UI_ALIGN_CENTER ,
				.layout.margin.x = 15 },
				OC_UI_STYLE_SIZE
				| OC_UI_STYLE_LAYOUT_AXIS
				| OC_UI_STYLE_LAYOUT_ALIGN_X
				| OC_UI_STYLE_LAYOUT_ALIGN_Y
				| OC_UI_STYLE_LAYOUT_MARGIN_X);
			oc_ui_container("menu bar right", OC_UI_FLAG_NONE)
			{
				oc_ui_style_next(&(oc_ui_style){ .layout.margin.x = 15 }, OC_UI_STYLE_LAYOUT_MARGIN_X);
				oc_ui_label(game.score_string);

				oc_ui_style_next(&(oc_ui_style){ .layout.margin.x = 15 }, OC_UI_STYLE_LAYOUT_MARGIN_X);
				oc_ui_label(game.moves_string);

				oc_ui_style_next(&(oc_ui_style){ .layout.margin.x = 15 }, OC_UI_STYLE_LAYOUT_MARGIN_X);
				oc_ui_label(game.timer_string);
			}
		}

		if (game.state == STATE_SELECT_CARD_BACK) {
			do_card_back_menu();
		}
	}

	assert(menu);
	game.menu_opened = !oc_ui_box_closed(menu);
}

ORCA_EXPORT void oc_on_init(void) {
	// initialize random number generator
	f64 ftime = oc_clock_time(OC_CLOCK_MONOTONIC);
	u64 time = *((u64*)&ftime);
	pcg32_init(time);

    oc_window_set_title(OC_STR8("Solitaire"));
    game.surface = oc_surface_canvas();
    game.canvas = oc_canvas_create();

	game.draw_three_mode = true;

    oc_unicode_range ranges[5] = {
        OC_UNICODE_BASIC_LATIN,
        OC_UNICODE_C1_CONTROLS_AND_LATIN_1_SUPPLEMENT,
        OC_UNICODE_LATIN_EXTENDED_A,
        OC_UNICODE_LATIN_EXTENDED_B,
        OC_UNICODE_SPECIALS,
    };
	game.font = oc_font_create_from_path(OC_STR8("segoeui.ttf"), 5, ranges);

	game.bg_color = (oc_color){ 10.0f/255.0f, 31.0f/255.0f, 72.0f/255.0f, 1 };
	game.menu_bg_color = (oc_color){ 12.0f/255.0f, 41.0f/255.0f, 80.0f/255.0f, 1 };
	game.menu_card_backs_margin = 25;

	game.timer = 0;
	update_timer_string(game.timer);

	update_moves_string();

	UpdateScoreParams params = { .kind = SCORE_RESET };
	update_score(params);
	load_highscore();

	oc_vec2 viewport_size = { 1000, 775 };
	set_sizes_based_on_viewport(viewport_size.x, viewport_size.y);
	oc_window_set_size(viewport_size);

	oc_ui_init(&game.ui);

	load_images();
	game.selected_card_back = 0;

	game.stock.kind = PILE_STOCK;
	game.waste.kind = PILE_WASTE;
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		game.foundations[i].kind = PILE_FOUNDATION;
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		game.tableau[i].kind = PILE_TABLEAU;
	}

    game.last_timestamp = oc_clock_time(OC_CLOCK_DATE);

	game.deal_countdown = 0;
	game.deal_delay = 0.1;
	game.deal_tableau_index = 0;
	game.deal_tableau_remaining = ARRAY_COUNT(game.tableau);
	game.deal_cards_remaining = 28; // 7+6+5+4+3+2+1

	game.deal_speed = 10;
	game.card_animate_speed = 25;
	deal_klondike(game.cards, ARRAY_COUNT(game.cards));
}

ORCA_EXPORT void oc_on_resize(u32 width, u32 height) {
	oc_log_info("width=%lu height=%lu", width, height);

	set_sizes_based_on_viewport(width, height);

	position_all_cards_on_pile(&game.stock, false);
	position_all_cards_on_pile(&game.waste, false);
	for (i32 i=0; i<ARRAY_COUNT(game.foundations); ++i) {
		position_all_cards_on_pile(&game.foundations[i], false);
	}
	for (i32 i=0; i<ARRAY_COUNT(game.tableau); ++i) {
		position_all_cards_on_pile(&game.tableau[i], false);
	}
}

ORCA_EXPORT void oc_on_key_down(oc_scan_code scan, oc_key_code key) {
	switch (key) {
	case OC_KEY_R: game.input.r.down = true;    break;
	case OC_KEY_U: game.input.u.down = true;    break;
	case OC_KEY_1: game.input.num1.down = true; break;
	case OC_KEY_2: game.input.num2.down = true; break;
	case OC_KEY_3: game.input.num3.down = true; break;
	case OC_KEY_4: game.input.num4.down = true; break;
	case OC_KEY_5: game.input.num5.down = true; break;
	case OC_KEY_6: game.input.num6.down = true; break;
	case OC_KEY_7: game.input.num7.down = true; break;
	case OC_KEY_8: game.input.num8.down = true; break;
	case OC_KEY_9: game.input.num9.down = true; break;
	case OC_KEY_0: game.input.num0.down = true; break;
	default:
		break;
	}

}

ORCA_EXPORT void oc_on_key_up(oc_scan_code scan, oc_key_code key) {
	switch (key) {
	case OC_KEY_R: game.input.r.down = false;    break;
	case OC_KEY_U: game.input.u.down = false;    break;
	case OC_KEY_1: game.input.num1.down = false; break;
	case OC_KEY_2: game.input.num2.down = false; break;
	case OC_KEY_3: game.input.num3.down = false; break;
	case OC_KEY_4: game.input.num4.down = false; break;
	case OC_KEY_5: game.input.num5.down = false; break;
	case OC_KEY_6: game.input.num6.down = false; break;
	case OC_KEY_7: game.input.num7.down = false; break;
	case OC_KEY_8: game.input.num8.down = false; break;
	case OC_KEY_9: game.input.num9.down = false; break;
	case OC_KEY_0: game.input.num0.down = false; break;
	default:
		break;
	}
}

ORCA_EXPORT void oc_on_mouse_down(int button) {
	if (button == OC_MOUSE_LEFT) {
		game.mouse_input.left.down = true;
	} else if (button == OC_MOUSE_RIGHT) {
		game.mouse_input.right.down = true;
	}
}

ORCA_EXPORT void oc_on_mouse_up(int button) {
	if (button == OC_MOUSE_LEFT) {
		game.mouse_input.left.down = false;
	} else if (button == OC_MOUSE_RIGHT) {
		game.mouse_input.right.down = false;
	}
}

ORCA_EXPORT void oc_on_mouse_move(float x, float y, float dx, float dy) {
    game.mouse_input.x = x;
    game.mouse_input.y = y;
    game.mouse_input.deltaX = dx;
    game.mouse_input.deltaY = dy;
}

ORCA_EXPORT void oc_on_raw_event(oc_event* event) {
    oc_ui_process_event(event);
}

ORCA_EXPORT void oc_on_frame_refresh(void) {
    f64 timestamp = oc_clock_time(OC_CLOCK_DATE);
	game.dt = timestamp - game.last_timestamp;
	game.last_timestamp = timestamp;

	solitaire_menu();
	solitaire_update();
	solitaire_draw();
}


