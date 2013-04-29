#ifndef TBPROBE_H
#define TBPROBE_H

extern int tb_max_pieces_found;
void init_tablebases(void);
int probe_wdl(Position& pos, int *success);
int probe_dtz(Position& pos, int *success);
int root_probe(Position& pos, Move *move, int *success);

#endif

