#ifndef ___PB_EEPROM__H___
#define ___PB_EEPROM__H___

#include <avr/eeprom.h>
#include <pb_cpp_api.h>

#ifndef eeprom_update_block
/* Older versions of `avr-gcc` (like the one included with Arduino IDE 1.0.5)
 * do not include the `eeprom_update_block` function. Use `eeprom_write_block`
 * instead. */
static __inline__ void
eeprom_update_block (const void *__src, void *__dst, size_t __n) {
  eeprom_write_block(__src, __dst, __n);
}
#endif


namespace nanopb {

inline UInt8Array eeprom_to_array(uint16_t address, UInt8Array output) {
  uint16_t payload_size;

  eeprom_read_block((void*)&payload_size, (void*)address, sizeof(uint16_t));
  if (output.length < payload_size) {
    output.length = 0;
    output.data = NULL;
  } else {
    eeprom_read_block((void*)output.data, (void*)(address + 2), payload_size);
    output.length = payload_size;
  }
  return output;
}


inline void array_to_eeprom(uint16_t address, UInt8Array data) {
  cli();
  eeprom_update_block((void*)&data.length, (void*)address, sizeof(uint16_t));
  eeprom_update_block((void*)data.data, (void*)(address + 2), data.length);
  sei();
}


template <typename Obj, typename Fields>
bool decode_obj_from_eeprom(uint16_t address, Obj &obj,
                            Fields const &fields, UInt8Array pb_buffer) {
  pb_buffer = eeprom_to_array(address, pb_buffer);
  bool ok;
  if (pb_buffer.data == NULL) {
    ok = false;
  } else {
    ok = decode_from_array(pb_buffer, fields, obj);
  }
  return ok;
}


template <typename Msg, typename Validator>
class EepromMessage : public Message<Msg, Validator> {
public:
  typedef Message<Msg, Validator> base_type;
  using base_type::_;
  using base_type::fields_;
  using base_type::buffer_;
  using base_type::reset;
  using base_type::serialize;
  using base_type::validate;

  EepromMessage(const pb_field_t *fields) : base_type(fields) {}

  EepromMessage(const pb_field_t *fields, size_t buffer_size, uint8_t *buffer)
    : base_type(fields, buffer_size, buffer) {}
  EepromMessage(const pb_field_t *fields, UInt8Array buffer)
    : base_type(fields, buffer) {}

  void load(uint8_t address=0) {
    if (!decode_obj_from_eeprom(address, _, fields_, buffer_)) {
      /* Message could not be loaded from EEPROM; reset obj. */
      reset();
    }
    validate();
  }
  void save(uint8_t address=0) {
    UInt8Array serialized = serialize();
    if (serialized.data != NULL) {
      array_to_eeprom(address, serialized);
    }
  }
};


} // namespace nanopb

#endif  // #ifndef ___PB_EEPROM__H___
