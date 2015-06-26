#ifndef ___PB_FIELD_VALIDATOR__H___
#define ___PB_FIELD_VALIDATOR__H___

#include <pb_message_update.h>


struct FieldHandlerBase {
    virtual int8_t set_tag(uint8_t level, uint8_t value) = 0;
    virtual bool __validate__(void *a, void *b, size_t data_size) = 0;
    virtual bool match(MessageUpdateBase::Parent *parents, size_t parent_count,
                       pb_field_iter_t &iter) = 0;
};


template <typename T, uint8_t Level=1>
struct ScalarFieldHandler : public FieldHandlerBase {
  uint8_t tags_[Level];

  virtual int8_t set_tag(uint8_t level, uint8_t value) {
    if (level > Level) { return -1; }
    this->tags_[level] = value;
  }

  bool match(MessageUpdateBase::Parent *parents, size_t parent_count,
             pb_field_iter_t &iter) {
    if (Level != parent_count + 1) {
      LOG("  Level=%d != parent_count + 1=%d\n", Level, parent_count + 1);
      return false;
    }
    int i;
    for (i = 0; i < parent_count; i++) {
      if (parents[i].pos->tag != tags_[i]) {
        LOG("  parent_tag=%d != tags_=%d\n", parents[i].pos->tag, tags_[i]);
        return false;
      }
    }
    if (tags_[i] != iter.pos->tag) { return false; }
    LOG("  match\n");
    return true;
  }
  bool __validate__(void *a, void *b, size_t data_size) {
      if (data_size != sizeof(T)) {
          LOG("  **Incorrect size: data_size=%d, expected arg size=%d\n",
              data_size, sizeof(T));
          return false;
      }
      T &a_ = *((T *)a);
      T &b_ = *((T *)b);
      return this->validate(a_, b_);
  }
  virtual bool validate(T &a, T &b) = 0;
};


template <uint8_t HandlerCount>
struct FieldValidator : public MessageUpdateBase {
  using MessageUpdateBase::IterPair;

  FieldHandlerBase *handlers[HandlerCount];

  FieldValidator<HandlerCount> () : MessageUpdateBase() {
    for (int i = 0; i < HandlerCount; i++) { handlers[i] = NULL; }
  }

  virtual uint8_t register_handler(FieldHandlerBase &handler) {
    for (int i = 0; i < HandlerCount; i++) {
      if (handlers[i] == NULL) {
        handlers[i] = &handler;
        return i;
      }
    }
    return -1;
  }

  virtual bool process_field(IterPair &iter, pb_size_t count) {
    LOG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
    bool trigger_copy = false;
    for (int i = 0; i < sizeof(handlers) / sizeof(FieldHandlerBase *); i++) {
      /* If `count==0` field is not set in source message. */
      if (count > 0 && handlers[i] != NULL &&
          handlers[i]->match(parents, parent_count, iter.source)) {
        LOG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
        trigger_copy = handlers[i]->__validate__(iter.source.pData,
                                                 iter.target.pData,
                                                 iter.source.pos->data_size);
      }
    }
#ifdef LOGGING
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("=========================================\n");
    for (int i = 0; i < parent_count; i++) LOG("  ");
    if (parent_count > 0) {
      for (int i = 0; i < parent_count; i++) {
        LOG("> %d ", parents[i].pos->tag);
      }
      LOG("\n");
    }
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("tag=%d start=%p pos=%p count=%d ltype=%x atype=%x htype=%x data_size=%d \n",
        iter.source.pos->tag, iter.source.start, iter.source.pos, count,
        PB_LTYPE(iter.source.pos->type), PB_ATYPE(iter.source.pos->type),
        PB_HTYPE(iter.source.pos->type), iter.source.pos->data_size);
    for (int i = 0; i < parent_count; i++) LOG("  ");
    LOG("-----------------------------------------");

    if (PB_LTYPE(iter.source.pos->type) != PB_LTYPE_SUBMESSAGE) {
      /* Only copy all data for field if this is not a sub-message type, since
       * we want to handle sub-message fields one-by-one. */
      trigger_copy = (count > 0);
    }
    LOG("%c\n", (trigger_copy) ? 'W' : ' ');
#endif
    return trigger_copy;
  }
};


#endif  // #ifndef ___PB_FIELD_VALIDATOR__H___
