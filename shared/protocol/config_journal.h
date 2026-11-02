#ifndef CONFIG_JOURNAL_H
#define CONFIG_JOURNAL_H
#include <stdbool.h>
#include <stdint.h>
#define CJ_KEYS 17U
#define CJ_RECORD_BYTES 41U
typedef bool (*CjRead)(uint8_t address, uint8_t *value);
typedef bool (*CjWrite)(uint8_t address, uint8_t value);
typedef struct { uint8_t values[CJ_KEYS * 2]; uint16_t sequence; int active; bool loaded; } ConfigJournal;
extern const uint8_t Cj_Addresses[CJ_KEYS];
bool Cj_Load(ConfigJournal *j, CjRead read);
bool Cj_Set(ConfigJournal *j, CjRead read, CjWrite write, uint8_t key, uint16_t value);
bool Cj_SetMany(ConfigJournal *j, CjRead read, CjWrite write, const uint8_t *keys, const uint16_t *values, unsigned count);
uint16_t Cj_Get(const ConfigJournal *j, uint8_t key);
int Cj_Key(uint8_t address);
#endif
