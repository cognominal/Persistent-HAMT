#include "moar.h"
#include "limits.h"

/* This representation's function pointer table. */
static const MVMREPROps VMArray_this_repr;

MVM_STATIC_INLINE void enter_single_user(MVMThreadContext *tc, MVMArrayBody *arr) {
#if MVM_ARRAY_CONC_DEBUG
    if (!MVM_trycas(&(arr->in_use), 0, 1)) {
        MVM_dump_backtrace(tc);
        MVM_exception_throw_adhoc(tc, "Array may not be used concurrently");
    }
#endif
}
static void exit_single_user(MVMThreadContext *tc, MVMArrayBody *arr) {
#if MVM_ARRAY_CONC_DEBUG
    arr->in_use = 0;
#endif
}

#define MVM_MAX(a,b) ((a)>(b)?(a):(b))
#define MVM_MIN(a,b) ((a)<(b)?(a):(b))

/* Creates a new type object of this representation, and associates it with
 * the given HOW. */
static MVMObject * type_object_for(MVMThreadContext *tc, MVMObject *HOW) {
    MVMSTable        *st = MVM_gc_allocate_stable(tc, &VMArray_this_repr, HOW);

    MVMROOT(tc, st, {
        MVMObject *obj = MVM_gc_allocate_type_object(tc, st);
        MVMArrayREPRData *repr_data = (MVMArrayREPRData *)MVM_malloc(sizeof(MVMArrayREPRData));

        repr_data->slot_type = MVM_ARRAY_OBJ;
        repr_data->elem_size = sizeof(MVMObject *);
        repr_data->elem_type = NULL;

        MVM_ASSIGN_REF(tc, &(st->header), st->WHAT, obj);
        st->size = sizeof(MVMArray);
        st->REPR_data = repr_data;
    });

    return st->WHAT;
}

/* Copies the body of one object to another. The result has the space
 * needed for the current number of elements, which may not be the
 * entire allocated slot size. */
static void copy_to(MVMThreadContext *tc, MVMSTable *st, void *src, MVMObject *dest_root, void *dest) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVMArrayBody     *src_body  = (MVMArrayBody *)src;
    MVMArrayBody     *dest_body = (MVMArrayBody *)dest;
    dest_body->elems = src_body->elems;
    dest_body->ssize = src_body->elems;
    dest_body->start = 0;
    if (dest_body->elems > 0) {
        size_t  mem_size     = dest_body->ssize * repr_data->elem_size;
        size_t  start_pos    = src_body->start * repr_data->elem_size;
        char   *copy_start   = ((char *)src_body->slots.any) + start_pos;
        dest_body->slots.any = MVM_malloc(mem_size);
        memcpy(dest_body->slots.any, copy_start, mem_size);
    }
    else {
        dest_body->slots.any = NULL;
    }
}

/* Adds held objects to the GC worklist. */
static void VMArray_gc_mark(MVMThreadContext *tc, MVMSTable *st, void *data, MVMGCWorklist *worklist) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64         elems     = body->elems;
    MVMuint64         start     = body->start;
    MVMuint64         i         = 0;

    /* Aren't holding anything, nothing to do. */
    if (elems == 0)
        return;

    switch (repr_data->slot_type) {
        case MVM_ARRAY_OBJ: {
            MVMObject **slots = body->slots.o;
            slots += start;
            MVM_gc_worklist_presize_for(tc, worklist, elems);
            if (worklist->include_gen2) {
                for (; i < elems; i++)
                    MVM_gc_worklist_add_include_gen2_nocheck(tc, worklist, &slots[i]);
            }
            else {
                for (; i < elems; i++)
                    MVM_gc_worklist_add_no_include_gen2_nocheck(tc, worklist, &slots[i]);
            }
            break;
        }
        case MVM_ARRAY_STR: {
            MVMString **slots = body->slots.s;
            slots += start;
            MVM_gc_worklist_presize_for(tc, worklist, elems);
            if (worklist->include_gen2) {
                for (; i < elems; i++)
                    MVM_gc_worklist_add_include_gen2_nocheck(tc, worklist, &slots[i]);
            }
            else {
                for (; i < elems; i++)
                    MVM_gc_worklist_add_no_include_gen2_nocheck(tc, worklist, &slots[i]);
            }
            break;
        }
    }
}

/* Called by the VM in order to free memory associated with this object. */
static void gc_free(MVMThreadContext *tc, MVMObject *obj) {
    MVMArray *arr = (MVMArray *)obj;
    MVM_free(arr->body.slots.any);
}

/* Marks the representation data in an STable.*/
static void gc_mark_repr_data(MVMThreadContext *tc, MVMSTable *st, MVMGCWorklist *worklist) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    if (repr_data == NULL)
        return;
    MVM_gc_worklist_add(tc, worklist, &repr_data->elem_type);
}

/* Frees the representation data in an STable.*/
static void gc_free_repr_data(MVMThreadContext *tc, MVMSTable *st) {
    MVM_free(st->REPR_data);
}


static const MVMStorageSpec storage_spec = {
    MVM_STORAGE_SPEC_REFERENCE, /* inlineable */
    0,                          /* bits */
    0,                          /* align */
    MVM_STORAGE_SPEC_BP_NONE,   /* boxed_primitive */
    0,                          /* can_box */
    0,                          /* is_unsigned */
};


/* Gets the storage specification for this representation. */
static const MVMStorageSpec * get_storage_spec(MVMThreadContext *tc, MVMSTable *st) {
    return &storage_spec;
}

void MVM_VMArray_at_pos(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMint64 index, MVMRegister *value, MVMuint16 kind) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64        real_index;

    /* Handle negative indexes. */
    if (index < 0) {
        index += body->elems;
        if (index < 0)
            MVM_exception_throw_adhoc(tc, "MVMArray: Index out of bounds");
    }

    real_index = (MVMuint64)index;

    /* Go by type. */
    switch (repr_data->slot_type) {
        case MVM_ARRAY_OBJ:
            if (kind != MVM_reg_obj)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos expected object register");
            if (real_index >= body->elems) {
                value->o = tc->instance->VMNull;
            }
            else {
                MVMObject *found = body->slots.o[body->start + real_index];
                value->o = found ? found : tc->instance->VMNull;
            }
            break;
        case MVM_ARRAY_STR:
            if (kind != MVM_reg_str)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos expected string register");
            if (real_index >= body->elems)
                value->s = NULL;
            else
                value->s = body->slots.s[body->start + real_index];
            break;
        case MVM_ARRAY_I64:
            if (kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos I64 expected int register");
            if (real_index >= body->elems)
                value->i64 = 0;
            else
                value->i64 = (MVMint64)body->slots.i64[body->start + real_index];
            break;
        case MVM_ARRAY_I32:
            if (kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos I32 expected int register");
            if (real_index >= body->elems)
                value->i64 = 0;
            else
                value->i64 = (MVMint64)body->slots.i32[body->start + real_index];
            break;
        case MVM_ARRAY_I16:
            if (kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos I16 expected int register");
            if (real_index >= body->elems)
                value->i64 = 0;
            else
                value->i64 = (MVMint64)body->slots.i16[body->start + real_index];
            break;
        case MVM_ARRAY_I8:
            if (kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos I8 expected int register");
            if (real_index >= body->elems)
                value->i64 = 0;
            else
                value->i64 = (MVMint64)body->slots.i8[body->start + real_index];
            break;
        case MVM_ARRAY_N64:
            if (kind != MVM_reg_num64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos expected num register");
            if (real_index >= body->elems)
                value->n64 = 0.0;
            else
                value->n64 = (MVMnum64)body->slots.n64[body->start + real_index];
            break;
        case MVM_ARRAY_N32:
            if (kind != MVM_reg_num64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos expected num register");
            if (real_index >= body->elems)
                value->n64 = 0.0;
            else
                value->n64 = (MVMnum64)body->slots.n32[body->start + real_index];
            break;
        case MVM_ARRAY_U64:
            if (kind != MVM_reg_uint64 && kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos U64 expected int register, got %d instead", kind);
            if (real_index >= body->elems)
                value->u64 = 0;
            else
                value->u64 = (MVMint64)body->slots.u64[body->start + real_index];
            break;
        case MVM_ARRAY_U32:
            if (kind != MVM_reg_uint64 && kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos U32 expected int register");
            if (real_index >= body->elems)
                value->u64 = 0;
            else
                value->u64 = (MVMint64)body->slots.u32[body->start + real_index];
            break;
        case MVM_ARRAY_U16:
            if (kind != MVM_reg_uint64 && kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos U16 expected int register");
            if (real_index >= body->elems)
                value->u64 = 0;
            else
                value->u64 = (MVMint64)body->slots.u16[body->start + real_index];
            break;
        case MVM_ARRAY_U8:
            if (kind != MVM_reg_uint64 && kind != MVM_reg_int64)
                MVM_exception_throw_adhoc(tc, "MVMArray: atpos U8 expected int register");
            if (real_index >= body->elems)
                value->u64 = 0;
            else
                value->u64 = (MVMint64)body->slots.u8[body->start + real_index];
            break;
        default:
            MVM_exception_throw_adhoc(tc, "MVMArray: Unhandled slot type, got '%s'", MVM_reg_get_debug_name(tc, repr_data->slot_type));
    }
}

static MVMuint64 zero_slots(MVMThreadContext *tc, MVMArrayBody *body,
        MVMuint64 elems, MVMuint64 ssize, MVMuint8 slot_type) {
    switch (slot_type) {
        case MVM_ARRAY_OBJ:
            memset(&(body->slots.o[elems]), 0, (ssize - elems) * sizeof(MVMObject *));
            break;
        case MVM_ARRAY_STR:
            memset(&(body->slots.s[elems]), 0, (ssize - elems) * sizeof(MVMString *));
            break;
        case MVM_ARRAY_I64:
            memset(&(body->slots.i64[elems]), 0, (ssize - elems) * sizeof(MVMint64));
            break;
        case MVM_ARRAY_I32:
            memset(&(body->slots.i32[elems]), 0, (ssize - elems) * sizeof(MVMint32));
            break;
        case MVM_ARRAY_I16:
            memset(&(body->slots.i16[elems]), 0, (ssize - elems) * sizeof(MVMint16));
            break;
        case MVM_ARRAY_I8:
            memset(&(body->slots.i8[elems]), 0, (ssize - elems) * sizeof(MVMint8));
            break;
        case MVM_ARRAY_N64:
            memset(&(body->slots.n64[elems]), 0, (ssize - elems) * sizeof(MVMnum64));
            break;
        case MVM_ARRAY_N32:
            memset(&(body->slots.n32[elems]), 0, (ssize - elems) * sizeof(MVMnum32));
            break;
        case MVM_ARRAY_U64:
            memset(&(body->slots.u64[elems]), 0, (ssize - elems) * sizeof(MVMuint64));
            break;
        case MVM_ARRAY_U32:
            memset(&(body->slots.u32[elems]), 0, (ssize - elems) * sizeof(MVMuint32));
            break;
        case MVM_ARRAY_U16:
            memset(&(body->slots.u16[elems]), 0, (ssize - elems) * sizeof(MVMuint16));
            break;
        case MVM_ARRAY_U8:
            memset(&(body->slots.u8[elems]), 0, (ssize - elems) * sizeof(MVMuint8));
            break;
        default:
            MVM_exception_throw_adhoc(tc, "MVMArray: Unhandled slot type");
    }
    return elems;
}

static void set_size_internal(MVMThreadContext *tc, MVMArrayBody *body, MVMuint64 n, MVMArrayREPRData *repr_data) {
    MVMuint64   elems = body->elems;
    MVMuint64   start = body->start;
    MVMuint64   ssize = body->ssize;
    void       *slots = body->slots.any;

    if (n == elems)
        return;

    if (start > 0 && n + start > ssize) {
        /* if there aren't enough slots at the end, shift off empty slots
         * from the beginning first */
        if (elems > 0)
            memmove(slots,
                (char *)slots + start * repr_data->elem_size,
                elems * repr_data->elem_size);
        body->start = 0;
        /* fill out any unused slots with NULL pointers or zero values */
        zero_slots(tc, body, elems, start+elems, repr_data->slot_type);
        elems = ssize; /* we'll use this as a point to clear from later */
    }
    else if (n < elems) {
        /* we're downsizing; clear off extra slots */
        zero_slots(tc, body, n+start, start+elems, repr_data->slot_type);
    }

    if (n <= ssize) {
        /* we already have n slots available, we can just return */
        body->elems = n;
        return;
    }

    /* We need more slots.  If the current slot size is less
     * than 8K, use the larger of twice the current slot size
     * or the actual number of elements needed.  Otherwise,
     * grow the slots to the next multiple of 4096 (0x1000). */
    if (ssize < 8192) {
        ssize *= 2;
        if (n > ssize) ssize = n;
        if (ssize < 8) ssize = 8;
    }
    else {
        ssize = (n + 0x1000) & ~0xfffUL;
    }
    {
        /* Our budget is 2^(
         *     <number of bits in an array index>
         *     - <number of bits to address individual bytes in an array element>
         * ) */
        size_t const elem_addr_size = repr_data->elem_size == 8 ? 4 :
                                      repr_data->elem_size == 4 ? 3 :
                                      repr_data->elem_size == 2 ? 2 :
                                                                  1;
        if (ssize > (1ULL << (CHAR_BIT * sizeof(size_t) - elem_addr_size)))
            MVM_exception_throw_adhoc(tc,
                "Unable to allocate an array of %"PRIu64" elements",
                ssize);
    }

    /* now allocate the new slot buffer */
    slots = (slots)
            ? MVM_realloc(slots, ssize * repr_data->elem_size)
            : MVM_malloc(ssize * repr_data->elem_size);

    /* fill out any unused slots with NULL pointers or zero values */
    body->slots.any = slots;
    zero_slots(tc, body, elems, ssize, repr_data->slot_type);

    body->ssize = ssize;
    /* set elems last so no thread tries to access slots before they are available */
    body->elems = n;
}

void MVM_VMArray_bind_pos(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMint64 index, MVMRegister value, MVMuint16 kind) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64        real_index;

}


static MVMStorageSpec get_elem_storage_spec(MVMThreadContext *tc, MVMSTable *st) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVMStorageSpec spec;

    /* initialise storage spec to default values */
    spec.bits            = 0;
    spec.align           = 0;
    spec.is_unsigned     = 0;

    switch (repr_data->slot_type) {
        case MVM_ARRAY_STR:
            spec.inlineable      = MVM_STORAGE_SPEC_INLINED;
            spec.boxed_primitive = MVM_STORAGE_SPEC_BP_STR;
            spec.can_box         = MVM_STORAGE_SPEC_CAN_BOX_STR;
            break;
        case MVM_ARRAY_I64:
        case MVM_ARRAY_I32:
        case MVM_ARRAY_I16:
        case MVM_ARRAY_I8:
            spec.inlineable      = MVM_STORAGE_SPEC_INLINED;
            spec.boxed_primitive = MVM_STORAGE_SPEC_BP_INT;
            spec.can_box         = MVM_STORAGE_SPEC_CAN_BOX_INT;
            break;
        case MVM_ARRAY_N64:
        case MVM_ARRAY_N32:
            spec.inlineable      = MVM_STORAGE_SPEC_INLINED;
            spec.boxed_primitive = MVM_STORAGE_SPEC_BP_NUM;
            spec.can_box         = MVM_STORAGE_SPEC_CAN_BOX_NUM;
            break;
        case MVM_ARRAY_U64:
        case MVM_ARRAY_U32:
        case MVM_ARRAY_U16:
        case MVM_ARRAY_U8:
            spec.inlineable      = MVM_STORAGE_SPEC_INLINED;
            spec.boxed_primitive = MVM_STORAGE_SPEC_BP_UINT64;
            spec.can_box         = MVM_STORAGE_SPEC_CAN_BOX_INT;
            spec.is_unsigned     = 1;
            break;
        default:
            spec.inlineable      = MVM_STORAGE_SPEC_REFERENCE;
            spec.boxed_primitive = MVM_STORAGE_SPEC_BP_NONE;
            spec.can_box         = 0;
            break;
    }
    return spec;
}


/* Compose the representation. */
static void spec_to_repr_data(MVMThreadContext *tc, MVMArrayREPRData *repr_data, const MVMStorageSpec *spec) {
    switch (spec->boxed_primitive) {
        case MVM_STORAGE_SPEC_BP_UINT64:
        case MVM_STORAGE_SPEC_BP_INT:
            if (spec->is_unsigned) {
                switch (spec->bits) {
                    case 64:
                        repr_data->slot_type = MVM_ARRAY_U64;
                        repr_data->elem_size = sizeof(MVMuint64);
                        break;
                    case 32:
                        repr_data->slot_type = MVM_ARRAY_U32;
                        repr_data->elem_size = sizeof(MVMuint32);
                        break;
                    case 16:
                        repr_data->slot_type = MVM_ARRAY_U16;
                        repr_data->elem_size = sizeof(MVMuint16);
                        break;
                    case 8:
                        repr_data->slot_type = MVM_ARRAY_U8;
                        repr_data->elem_size = sizeof(MVMuint8);
                        break;
                    case 4:
                        repr_data->slot_type = MVM_ARRAY_U4;
                        repr_data->elem_size = 0;
                        break;
                    case 2:
                        repr_data->slot_type = MVM_ARRAY_U2;
                        repr_data->elem_size = 0;
                        break;
                    case 1:
                        repr_data->slot_type = MVM_ARRAY_U1;
                        repr_data->elem_size = 0;
                        break;
                    default:
                        MVM_exception_throw_adhoc(tc,
                            "MVMArray: Unsupported uint size");
                }
            }
            else {
                switch (spec->bits) {
                    case 64:
                        repr_data->slot_type = MVM_ARRAY_I64;
                        repr_data->elem_size = sizeof(MVMint64);
                        break;
                    case 32:
                        repr_data->slot_type = MVM_ARRAY_I32;
                        repr_data->elem_size = sizeof(MVMint32);
                        break;
                    case 16:
                        repr_data->slot_type = MVM_ARRAY_I16;
                        repr_data->elem_size = sizeof(MVMint16);
                        break;
                    case 8:
                        repr_data->slot_type = MVM_ARRAY_I8;
                        repr_data->elem_size = sizeof(MVMint8);
                        break;
                    case 4:
                        repr_data->slot_type = MVM_ARRAY_I4;
                        repr_data->elem_size = 0;
                        break;
                    case 2:
                        repr_data->slot_type = MVM_ARRAY_I2;
                        repr_data->elem_size = 0;
                        break;
                    case 1:
                        repr_data->slot_type = MVM_ARRAY_I1;
                        repr_data->elem_size = 0;
                        break;
                    default:
                        MVM_exception_throw_adhoc(tc,
                            "MVMArray: Unsupported int size");
                }
            }
            break;
        case MVM_STORAGE_SPEC_BP_NUM:
            switch (spec->bits) {
                case 64:
                    repr_data->slot_type = MVM_ARRAY_N64;
                    repr_data->elem_size = sizeof(MVMnum64);
                    break;
                case 32:
                    repr_data->slot_type = MVM_ARRAY_N32;
                    repr_data->elem_size = sizeof(MVMnum32);
                    break;
                default:
                    MVM_exception_throw_adhoc(tc,
                        "MVMArray: Unsupported num size");
            }
            break;
        case MVM_STORAGE_SPEC_BP_STR:
            repr_data->slot_type = MVM_ARRAY_STR;
            repr_data->elem_size = sizeof(MVMString *);
            break;
    }
}
static void compose(MVMThreadContext *tc, MVMSTable *st, MVMObject *info_hash) {
    MVMStringConsts         str_consts = tc->instance->str_consts;
    MVMArrayREPRData * const repr_data = (MVMArrayREPRData *)st->REPR_data;

    MVMObject *info = MVM_repr_at_key_o(tc, info_hash, str_consts.array);
    if (!MVM_is_null(tc, info)) {
        MVMObject *type = MVM_repr_at_key_o(tc, info, str_consts.type);
        if (!MVM_is_null(tc, type)) {
            const MVMStorageSpec *spec = REPR(type)->get_storage_spec(tc, STABLE(type));
            MVM_ASSIGN_REF(tc, &(st->header), repr_data->elem_type, type);
            spec_to_repr_data(tc, repr_data, spec);
        }
    }
}

/* Set the size of the STable. */
static void deserialize_stable_size(MVMThreadContext *tc, MVMSTable *st, MVMSerializationReader *reader) {
    st->size = sizeof(MVMArray);
}

/* Serializes the REPR data. */
static void serialize_repr_data(MVMThreadContext *tc, MVMSTable *st, MVMSerializationWriter *writer) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)st->REPR_data;
    MVM_serialization_write_ref(tc, writer, repr_data->elem_type);
}

/* Deserializes representation data. */
static void deserialize_repr_data(MVMThreadContext *tc, MVMSTable *st, MVMSerializationReader *reader) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *)MVM_malloc(sizeof(MVMArrayREPRData));

    MVMObject *type = MVM_serialization_read_ref(tc, reader);
    MVM_ASSIGN_REF(tc, &(st->header), repr_data->elem_type, type);
    repr_data->slot_type = MVM_ARRAY_OBJ;
    repr_data->elem_size = sizeof(MVMObject *);
    st->REPR_data = repr_data;

    if (type) {
        const MVMStorageSpec *spec;
        MVM_serialization_force_stable(tc, reader, STABLE(type));
        spec = REPR(type)->get_storage_spec(tc, STABLE(type));
        spec_to_repr_data(tc, repr_data, spec);
    }
}

static void deserialize(MVMThreadContext *tc, MVMSTable *st, MVMObject *root, void *data, MVMSerializationReader *reader) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *) st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64 i;

}

static void serialize(MVMThreadContext *tc, MVMSTable *st, void *data, MVMSerializationWriter *writer) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *) st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64 i;

}


/* Calculates the non-GC-managed memory we hold on to. */
static MVMuint64 unmanaged_size(MVMThreadContext *tc, MVMSTable *st, void *data) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *) st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    return body->ssize * repr_data->elem_size;
}

static void describe_refs (MVMThreadContext *tc, MVMHeapSnapshotState *ss, MVMSTable *st, void *data) {
    MVMArrayREPRData *repr_data = (MVMArrayREPRData *) st->REPR_data;
    MVMArrayBody     *body      = (MVMArrayBody *)data;
    MVMuint64         elems     = body->elems;
    MVMuint64         start     = body->start;
    MVMuint64         i         = 0;

    switch (repr_data->slot_type) {
        case MVM_ARRAY_OBJ: {
            MVMObject **slots = body->slots.o;
            slots += start;
            while (i < elems) {
                MVM_profile_heap_add_collectable_rel_idx(tc, ss,
                    (MVMCollectable *)slots[i], i);
                i++;
            }
            break;
        }
        case MVM_ARRAY_STR: {
            MVMString **slots = body->slots.s;
            slots += start;
            while (i < elems) {
                MVM_profile_heap_add_collectable_rel_idx(tc, ss,
                    (MVMCollectable *)slots[i], i);
                i++;
            }
            break;
        }
    }
}

/* Initializes the representation. */
const MVMREPROps * MVMArray_initialize(MVMThreadContext *tc) {
    return &VMArray_this_repr;
}

/static const MVMREPROps VMArray_this_repr = {
    type_object_for,
    MVM_gc_allocate_object, /* serialization.c relies on this and the next line */
    NULL, /* initialize */
    copy_to,
    MVM_REPR_DEFAULT_ATTR_FUNCS,
    MVM_REPR_DEFAULT_BOX_FUNCS,
    {
        MVM_VMArray_at_pos,
        MVM_VMArray_bind_pos,
        MVM_REPR_DEFAULT_SET_ELEMS,
        MVM_REPR_DEFAULT_PUSH,
        MVM_REPR_DEFAULT_POP,
        MVM_REPR_DEFAULT_UNSHIFT,
        MVM_REPR_DEFAULT_SHIFT,
        MVM_REPR_DEFAULT_SLICE,
        MVM_REPR_DEFAULT_SPLICE,
        MVM_REPR_DEFAULT_AT_POS_MULTIDIM,
        MVM_REPR_DEFAULT_BIND_POS_MULTIDIM,
        MVM_REPR_DEFAULT_DIMENSIONS,
        MVM_REPR_DEFAULT_SET_DIMENSIONS,
        MVM_REPR_DEFAULT_GET_ELEM_STORAGE_SPEC,
        MVM_REPR_DEFAULT_POS_AS_ATOMIC,
        MVM_REPR_DEFAULT_POS_AS_ATOMIC_MULTIDIM,
        MVM_REPR_DEFAULT_POS_WRITE_BUF,
        MVM_REPR_DEFAULT_POS_READ_BUF

    },    /* pos_funcs */
    MVM_REPR_DEFAULT_ASS_FUNCS,
    elems,
    get_storage_spec,
    NULL, /* change_type */
    serialize,
    deserialize,
    serialize_repr_data,
    deserialize_repr_data,
    deserialize_stable_size,
    VMArray_gc_mark,
    gc_free,
    NULL, /* gc_cleanup */
    gc_mark_repr_data,
    gc_free_repr_data,
    compose,
    spesh,
    "VMArray", /* name */
    MVM_REPR_ID_VMArray,
    unmanaged_size,
    describe_refs,
};
