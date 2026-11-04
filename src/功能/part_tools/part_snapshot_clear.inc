static void part_snapshot_clear(PartSnapshot *s){if(s->stamp)ProVerstampFree(&s->stamp);if(s->parent_stamp)ProVerstampFree(&s->parent_stamp);memset(s,0,sizeof(*s));}
