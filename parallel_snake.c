#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include "main.h"

/*
	Looks around to see wich neighbor has the same encoding, and sums them up.
	Return true if the sum divided by the encoding is at least 1.
*/

int check_if_tail(int **world, int i, int j, int encoding, int num_lines, int num_cols) {

	int sum = 0;
	int neighbor;

	if (i > 0) neighbor = world[i - 1][j];
	else neighbor = world[num_lines - 1][j];

	if (neighbor == encoding) sum+= neighbor;

	if (i < num_lines - 1) neighbor = world[i + 1][j];
	else neighbor = world[0][j];

	if (neighbor == encoding) sum+= neighbor;

	if (j > 0) neighbor = world[i][j - 1];
	else neighbor = world[i][num_cols - 1];

	if (neighbor == encoding) sum+= neighbor;

	if (j <  num_cols - 1) neighbor = world[i][j + 1];
	else neighbor = world[i][0];

	if (neighbor == encoding) sum+= neighbor;

	return sum / encoding > 1 ? 0 : 1;

}

/*
	Using the snake's direction, moves the snake's head towards it.
*/
void move_head(struct snake *snakes, int i, int num_lines, int num_cols) {

	switch(snakes[i].direction) {
		case 'N':
			if (snakes[i].head.line > 0) snakes[i].head.line--;
			else snakes[i].head.line = num_lines - 1;
			break;
		case 'E':
			if (snakes[i].head.col < num_cols - 1) snakes[i].head.col++;
			else snakes[i].head.col = 0;
			break;
		case 'S':
			if (snakes[i].head.line < num_lines - 1) snakes[i].head.line++;
			else snakes[i].head.line = 0;
			break;
		case 'V':
			if (snakes[i].head.col > 0) snakes[i].head.col--;
			else snakes[i].head.col = num_cols - 1;
			break;
	}

}

/*
	It finds out which is the first neighbor for the current position with the
	same encoding.
*/
struct coord which_neighbor_is_me(int **world, struct coord me, struct coord head, int num_lines, int num_cols) {

	struct coord n;
	n.line = me.line;
	n.col = me.col;

	if (me.col < num_cols - 1) n.col++;
		else n.col = 0;

	if (world[me.line][me.col] == world[n.line][n.col]
			&& (head.line != n.line || head.col != n.col)) {
		return n;
	}

	n.col = me.col;
	if (me.col > 0) n.col--;
	else n.col = num_cols - 1;

	if (world[me.line][me.col] == world[n.line][n.col]
			&& (head.line != n.line || head.col != n.col)) {
		return n;
	}

	n.col = me.col;
	if (me.line > 0) n.line--;
	else n.line = num_lines - 1;

	if (world[me.line][me.col] == world[n.line][n.col]
			&& (head.line != n.line || head.col != n.col)) {
		return n;
	}

	n.line = me.line;
	if (me.line < num_lines - 1) n.line++;
	else n.line = 0;
	if (world[me.line][me.col] == world[n.line][n.col]
			&& (head.line != n.line || head.col != n.col)) {
		return n;
	}

}

void copy_heads(struct coord *heads, struct snake *snakes, int num_snakes) {

	int i;
	#pragma omp parallel for private(i)
		for (i = 0; i < num_snakes; i++) {
			heads[i] = snakes[i].head;
		}

}

void copy_heads_back(struct snake *snakes, struct coord *heads, int num_snakes) {

	int i;
	#pragma omp parallel for private(i)
		for (i = 0 ;  i < num_snakes; i++) {
			snakes[i].head = heads[i];
		}

}

void copy_vectors(struct coord *vec1, struct coord *vec2, int num_snakes) {

	int i;
	#pragma omp parallel for private(i)
		for (i = 0;  i < num_snakes; i++) {
			vec1[i] = vec2[i];
		}

}

void run_simulation(int num_lines, int num_cols, int **world, int num_snakes,
		struct snake *snakes, int step_count, char *file_name)
{

	struct coord *tails = malloc(num_snakes * sizeof(struct coord));

	/* Checking the map to find out where the tails are positioned. */
  int l;
	#pragma omp parallel for private(l)
	for (l = 0;  l < num_snakes; l++) {
		int i, j;
		#pragma omp parallel for collapse(2) private (i, j)
		for (i = 0; i < num_lines; i++) {
			for (j = 0; j < num_cols; j++) {
					if (world[i][j] == snakes[l].encoding
						&& (snakes[l].head.line != i || snakes[l].head.col != j)) {
							if (check_if_tail(world, i, j, world[i][j], num_lines, num_cols)) {
								tails[l].line = i;
								tails[l].col = j;

							}
				}
			}
		}
	}

	int k;
	int stop = 0;

	struct coord *heads = malloc(num_snakes * sizeof(struct coord));
	struct coord *tails_copy = malloc(num_snakes * sizeof(struct coord));


	for (k = 0; k < step_count && !stop; k++) {
			copy_heads(heads, snakes, num_snakes);
			copy_vectors(tails_copy, tails, num_snakes);

			int l;
			#pragma omp parallel for private(l)
			for (l = 0; l < num_snakes; l++) {
					if (!stop) {

					move_head(snakes, l, num_lines, num_cols); // i move it's head
					if (world[snakes[l].head.line][snakes[l].head.col] == 0) {
						world[snakes[l].head.line][snakes[l].head.col] = snakes[l].encoding;
					} else { // resolve collision
						stop = 1;
					}

					// and i move it's tail
					if (!stop) {
						struct coord neighbor = which_neighbor_is_me(world, tails[l], snakes[l].head, num_lines, num_cols);
						world[tails[l].line][tails[l].col] = 0;
						tails[l].line = neighbor.line;
						tails[l].col = neighbor.col;
					}
				}
			}
	}

	if (stop) { // restore heads
		int l;
		#pragma omp parallel for private(l)
			for (l = 0; l < num_snakes; l++) {
				// restore matrix
				if (heads[l].line != snakes[l].head.line || heads[l].col != snakes[l].head.col) {
					if (world[snakes[l].head.line][snakes[l].head.col] == snakes[l].encoding) {
						world[snakes[l].head.line][snakes[l].head.col] = 0;
					}
					world[tails_copy[l].line][tails_copy[l].col] = snakes[l].encoding;
				}
			}
		copy_heads_back(snakes, heads, num_snakes);
	}

}
