#include "../hamt/internal_types.h"

typedef struct hamt_node MVMHamtNodeBody;

struct MVMHamtNodeREPRData {
    /* What type of slots we have. */
    MVMuint8 slot_type;
};


struct MVMHamtNode {
    MVMObject common;
    MVMHamtNodeBody body;
};
typedef struct MVMHamtNodeREPRData MVMHamtNodeREPRData;
typedef struct MVMHamtNode MVMHamtNode;