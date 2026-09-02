#!/usr/bin/env python3
from pathlib import Path
import re
import sqlite3
import sys

SERVER = Path(__file__).resolve().parents[1]
ROOT = SERVER.parent
fails=[]; passes=[]
def check(cond,label,detail=''):
    print(('[PASS] ' if cond else '[FAIL] ')+label+(f' :: {detail}' if detail else ''))
    (passes if cond else fails).append(label)

shared=(ROOT/'Shared'/'SpellDefines.h').read_text(errors='replace')
gamedata=(SERVER/'src'/'Database'/'GameData.cpp').read_text(errors='replace')
chardb=(SERVER/'src'/'Database'/'CharacterDb.cpp').read_text(errors='replace')
world=(SERVER/'src'/'Handlers'/'WorldHandlers.cpp').read_text(errors='replace')
npc=(SERVER/'src'/'AI'/'NpcAI.cpp').read_text(errors='replace')
npch=(SERVER/'src'/'AI'/'NpcAI.h').read_text(errors='replace')
spell=(SERVER/'src'/'Combat'/'SpellCaster.cpp').read_text(errors='replace')
utils=(SERVER/'src'/'Combat'/'SpellUtils.cpp').read_text(errors='replace')

for name,value in {'MeleeSpell':81,'RangedSpell':82,'LootUnit':85,'NpcGossip':88,'LootGameObj':127,'SleepRest':245,'Lockpicking':273}.items():
    check(re.search(rf'\b{name}\s*=\s*{value}\b',shared) is not None,f'static spell id {name}={value}')
check('Unit_Hostile = 14' in shared,'hostile spell target enum=14')
check('Target_GameObject = 13' in shared,'gameobject spell target enum=13')
check('SELECT class, spell FROM player_create_spell' in gamedata,'loads player_create_spell table')
check('ensureStartingSpells' in chardb and 'INSERT OR IGNORE INTO character_spells' in chardb,'starter-spell DB backfill exists')
check('ensureStartingSpells' in world,'login spellbook backfills existing characters')
check('NPC_MOVE_SPEED = 4.0f' in npch,'NPC movement uses logical 4 cells/sec basis')
check('moveSpeedPct / 100.0f' in npc,'NPC MoveSpeedPct converted to multiplier')
check('HOME_ARRIVAL_DISTANCE = 0.35f' in npch,'NPC home-arrival tolerance uses logical units')
check('GAME_DB_RANGE_UNITS_PER_CELL = 20.0f' in spell and '/ GAME_DB_RANGE_UNITS_PER_CELL' in spell,'spell DB distance is converted from asset units')

c=sqlite3.connect(ROOT/'game'/'game.db')
create=[r[0] for r in c.execute('SELECT spell FROM player_create_spell WHERE class=1 ORDER BY spell')]
check(create==[9,10,13,81,82,245,273],'class 1 starter spell DB set',str(create))
rows={r[0]:r[1:] for r in c.execute('SELECT entry,name,effect1_targetType,range FROM spell_template WHERE entry IN (9,10,13,81,82,245,273)')}
check(rows.get(81,(None,None,None))[1]==14 and rows.get(82,(None,None,None))[1]==14,'base attacks are hostile-unit spells')
check(rows.get(9,(None,None,None))[1]==14,'Holy Wrath is hostile-unit spell')

print(f'\nRESULT: {len(passes)} passed, {len(fails)} failed')
raise SystemExit(1 if fails else 0)
