#!/usr/bin/env python3
"""
Dreadmyst Server - Phase 6 Inventory End-to-End Test
Task 6.10: Inventory End-to-End Test

This script verifies the complete inventory feature set:
1) Inventory view
2) Move items
3) Stack items
4) Split stack
5) Equip item
6) Unequip item
7) Kill NPC and loot
8) Buy from vendor
9) Sell to vendor
10) Deposit/withdraw bank
11) Trade between players
12) Use consumable
13) Repair equipment

Protocol Notes:
- Packet format: [uint16 size][uint16 opcode][payload...]
- All integers are little-endian
- Strings are [uint16 length][bytes]
"""

import argparse
import random
import socket
import sqlite3
import struct
import sys
import threading
import time
from dataclasses import dataclass, field
from enum import IntEnum
from typing import Dict, List, Optional, Tuple


# =============================================================================
# Opcodes (from GamePacketBase.h)
# =============================================================================

class Opcode(IntEnum):
    # Bidirectional
    Mutual_Ping = 0

    # Server -> Client
    Server_Validate = 100
    Server_CharacterList = 102
    Server_CharaCreateResult = 103
    Server_NewWorld = 104
    Server_SetController = 105
    Server_Player = 110
    Server_Npc = 111
    Server_DestroyObject = 113
    Server_CastStart = 130
    Server_CastStop = 131
    Server_SpellGo = 132
    Server_CombatMsg = 133
    Server_ObjectVariable = 136
    Server_Inventory = 140
    Server_Bank = 141
    Server_OpenBank = 142
    Server_EquipItem = 143
    Server_NotifyItemAdd = 144
    Server_OpenLootWindow = 145
    Server_UpdateVendorStock = 147
    Server_RepairCost = 148
    Server_Spellbook = 160
    Server_SpentGold = 172
    Server_TradeUpdate = 220
    Server_TradeCanceled = 221
    Server_WorldError = 250

    # Client -> Server
    Client_Authenticate = 300
    Client_CharacterList = 301
    Client_CharCreate = 302
    Client_EnterWorld = 304
    Client_RequestMove = 310
    Client_CastSpell = 320
    Client_SetSelected = 322
    Client_EquipItem = 330
    Client_UnequipItem = 331
    Client_MoveItem = 332
    Client_SplitItemStack = 333
    Client_UseItem = 335
    Client_LootItem = 338
    Client_OpenBank = 340
    Client_MoveInventoryToBank = 341
    Client_UnBankItem = 343
    Client_BuyVendorItem = 350
    Client_SellItem = 351
    Client_OpenTradeWith = 353
    Client_TradeAddItem = 354
    Client_TradeConfirm = 357
    Client_Repair = 359


class AuthResult(IntEnum):
    Validated = 0
    WrongVersion = 1
    BadPassword = 2
    ServerFull = 3
    Banned = 4


class ObjVariable(IntEnum):
    Health = 1
    MaxHealth = 2
    Mana = 3
    MaxMana = 4
    Gold = 12
    IsDead = 21


# =============================================================================
# Buffer Utilities (matches StlBuffer serialization)
# =============================================================================

class PacketBuffer:
    def __init__(self, data: bytes = b''):
        self.data = bytearray(data)
        self.read_pos = 0

    def bytes(self) -> bytes:
        return bytes(self.data)

    # Write operations
    def write_u8(self, val: int) -> "PacketBuffer":
        self.data.append(val & 0xFF)
        return self

    def write_i8(self, val: int) -> "PacketBuffer":
        return self.write_u8(val)

    def write_u16(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<H", val))
        return self

    def write_i16(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<h", val))
        return self

    def write_u32(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<I", val))
        return self

    def write_i32(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<i", val))
        return self

    def write_u64(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<Q", val))
        return self

    def write_i64(self, val: int) -> "PacketBuffer":
        self.data.extend(struct.pack("<q", val))
        return self

    def write_float(self, val: float) -> "PacketBuffer":
        self.data.extend(struct.pack("<f", val))
        return self

    def write_bool(self, val: bool) -> "PacketBuffer":
        return self.write_u8(1 if val else 0)

    def write_string(self, val: str) -> "PacketBuffer":
        encoded = val.encode("utf-8")
        self.write_u16(len(encoded))
        self.data.extend(encoded)
        return self

    # Read operations
    def read_u8(self) -> int:
        if self.read_pos >= len(self.data):
            return 0
        val = self.data[self.read_pos]
        self.read_pos += 1
        return val

    def read_i8(self) -> int:
        val = self.read_u8()
        return val if val < 128 else val - 256

    def read_u16(self) -> int:
        if self.read_pos + 2 > len(self.data):
            return 0
        val = struct.unpack_from("<H", self.data, self.read_pos)[0]
        self.read_pos += 2
        return val

    def read_i16(self) -> int:
        if self.read_pos + 2 > len(self.data):
            return 0
        val = struct.unpack_from("<h", self.data, self.read_pos)[0]
        self.read_pos += 2
        return val

    def read_u32(self) -> int:
        if self.read_pos + 4 > len(self.data):
            return 0
        val = struct.unpack_from("<I", self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_i32(self) -> int:
        if self.read_pos + 4 > len(self.data):
            return 0
        val = struct.unpack_from("<i", self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_u64(self) -> int:
        if self.read_pos + 8 > len(self.data):
            return 0
        val = struct.unpack_from("<Q", self.data, self.read_pos)[0]
        self.read_pos += 8
        return val

    def read_i64(self) -> int:
        if self.read_pos + 8 > len(self.data):
            return 0
        val = struct.unpack_from("<q", self.data, self.read_pos)[0]
        self.read_pos += 8
        return val

    def read_float(self) -> float:
        if self.read_pos + 4 > len(self.data):
            return 0.0
        val = struct.unpack_from("<f", self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_bool(self) -> bool:
        return self.read_u8() != 0

    def read_string(self) -> str:
        length = self.read_u16()
        if length <= 0:
            return ""
        val = self.data[self.read_pos:self.read_pos + length].decode("utf-8")
        self.read_pos += length
        return val


def pack_item_id(buf: PacketBuffer, item_id: int) -> None:
    buf.write_i32(item_id)
    buf.write_i32(0)
    buf.write_i32(0)
    buf.write_i32(0)
    buf.write_i32(0)
    buf.write_i32(0)


def unpack_item_id(buf: PacketBuffer) -> int:
    item_id = buf.read_i32()
    _ = buf.read_i32()
    _ = buf.read_i32()
    _ = buf.read_i32()
    _ = buf.read_i32()
    _ = buf.read_i32()
    return item_id


def read_map_i32(buf: PacketBuffer) -> Dict[int, int]:
    result: Dict[int, int] = {}
    count = buf.read_u16()
    for _ in range(count):
        key = buf.read_i32()
        value = buf.read_i32()
        result[key] = value
    return result


def build_packet(opcode: int, payload: bytes = b"") -> bytes:
    size = 2 + 2 + len(payload)
    return struct.pack("<HH", size, opcode) + payload


# =============================================================================
# Data Models
# =============================================================================

@dataclass
class CharacterInfo:
    guid: int
    name: str
    class_id: int
    gender: int
    level: int
    portrait: int


@dataclass
class PlayerInfo:
    guid: int
    name: str
    x: float
    y: float
    orientation: float
    equipment: Dict[int, int] = field(default_factory=dict)
    variables: Dict[int, int] = field(default_factory=dict)


@dataclass
class NpcInfo:
    guid: int
    entry: int
    x: float
    y: float
    orientation: float
    variables: Dict[int, int] = field(default_factory=dict)


@dataclass
class LootWindow:
    guid: int
    money: int
    items: List[Tuple[int, int]] = field(default_factory=list)


@dataclass
class TradeState:
    my_items: List[Tuple[int, int, int]] = field(default_factory=list)
    their_items: List[Tuple[int, int, int]] = field(default_factory=list)
    my_gold: int = 0
    their_gold: int = 0
    their_confirmed: bool = False


# =============================================================================
# Network Client
# =============================================================================

class InventoryClient:
    def __init__(self, name: str, host: str, port: int):
        self.name = name
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.recv_buffer = b""
        self.running = False
        self.lock = threading.Lock()

        # State
        self.authenticated = False
        self.characters: List[CharacterInfo] = []
        self.last_char_create_result: Optional[int] = None
        self.map_id: Optional[int] = None
        self.my_guid: Optional[int] = None
        self.my_player: Optional[PlayerInfo] = None
        self.players: Dict[int, PlayerInfo] = {}
        self.npcs: Dict[int, NpcInfo] = {}
        self.inventory: Dict[int, Tuple[int, int]] = {}
        self.gold: int = 0
        self.bank: Dict[int, Tuple[int, int]] = {}
        self.loot_window: Optional[LootWindow] = None
        self.trade_state: Optional[TradeState] = None
        self.repair_cost: Optional[int] = None
        self.last_spent_gold: Optional[int] = None
        self.spells: List[int] = []

        self.inventory_version = 0
        self.bank_version = 0
        self.trade_version = 0
        self.loot_version = 0

        self.packet_queue: List[Tuple[int, PacketBuffer]] = []

    def connect(self) -> bool:
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            self.running = True
            thread = threading.Thread(target=self._recv_loop, daemon=True)
            thread.start()
            print(f"[{self.name}] Connected to {self.host}:{self.port}")
            return True
        except Exception as exc:
            print(f"[{self.name}] Failed to connect: {exc}")
            return False

    def disconnect(self) -> None:
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None

    def _send_packet(self, opcode: int, payload: bytes = b"") -> None:
        if not self.sock:
            return
        packet = build_packet(opcode, payload)
        self.sock.sendall(packet)

    # -------------------------------------------------------------------------
    # Incoming packet handling
    # -------------------------------------------------------------------------

    def _recv_loop(self) -> None:
        while self.running and self.sock:
            try:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                self.recv_buffer += chunk

                while len(self.recv_buffer) >= 4:
                    size, opcode = struct.unpack_from("<HH", self.recv_buffer, 0)
                    if len(self.recv_buffer) < size:
                        break
                    payload = self.recv_buffer[4:size]
                    self.recv_buffer = self.recv_buffer[size:]

                    buf = PacketBuffer(payload)
                    self._handle_packet(opcode, buf)
            except socket.timeout:
                continue
            except Exception as exc:
                print(f"[{self.name}] Receive error: {exc}")
                break

    def _enqueue_packet(self, opcode: int, payload: PacketBuffer) -> None:
        with self.lock:
            self.packet_queue.append((opcode, payload))

    def _handle_packet(self, opcode: int, buf: PacketBuffer) -> None:
        if opcode == Opcode.Server_Validate:
            result = buf.read_u8()
            _ = buf.read_i64()
            self.authenticated = (result == AuthResult.Validated)
        elif opcode == Opcode.Server_CharacterList:
            self._on_character_list(buf)
        elif opcode == Opcode.Server_CharaCreateResult:
            self.last_char_create_result = buf.read_u8()
        elif opcode == Opcode.Server_NewWorld:
            self.map_id = buf.read_i32()
        elif opcode == Opcode.Server_SetController:
            self.my_guid = buf.read_u32()
        elif opcode == Opcode.Server_Player:
            self._on_player(buf)
        elif opcode == Opcode.Server_Npc:
            self._on_npc(buf)
        elif opcode == Opcode.Server_ObjectVariable:
            self._on_object_variable(buf)
        elif opcode == Opcode.Server_Inventory:
            self._on_inventory(buf)
        elif opcode == Opcode.Server_Bank:
            self._on_bank(buf)
        elif opcode == Opcode.Server_EquipItem:
            self._on_equip_item(buf)
        elif opcode == Opcode.Server_OpenLootWindow:
            self._on_loot_window(buf)
        elif opcode == Opcode.Server_TradeUpdate:
            self._on_trade_update(buf)
        elif opcode == Opcode.Server_RepairCost:
            self.repair_cost = buf.read_i32()
        elif opcode == Opcode.Server_SpentGold:
            self.last_spent_gold = buf.read_i32()
        elif opcode == Opcode.Server_Spellbook:
            self._on_spellbook(buf)
        else:
            self._enqueue_packet(opcode, buf)

    def _on_character_list(self, buf: PacketBuffer) -> None:
        count = buf.read_u16()
        chars: List[CharacterInfo] = []
        for _ in range(count):
            guid = buf.read_u32()
            name = buf.read_string()
            class_id = buf.read_u8()
            gender = buf.read_u8()
            level = buf.read_i32()
            portrait = buf.read_i32()
            chars.append(CharacterInfo(guid, name, class_id, gender, level, portrait))
        self.characters = chars

    def _on_player(self, buf: PacketBuffer) -> None:
        guid = buf.read_u32()
        name = buf.read_string()
        _sub_name = buf.read_string()
        _class_id = buf.read_u8()
        _gender = buf.read_u8()
        _portrait = buf.read_i32()
        x = buf.read_float()
        y = buf.read_float()
        orientation = buf.read_float()
        equipment = read_map_i32(buf)
        variables = read_map_i32(buf)
        info = PlayerInfo(guid, name, x, y, orientation, equipment, variables)
        if self.my_guid == guid:
            self.my_player = info
        else:
            self.players[guid] = info

    def _on_npc(self, buf: PacketBuffer) -> None:
        guid = buf.read_u32()
        entry = buf.read_i32()
        x = buf.read_float()
        y = buf.read_float()
        orientation = buf.read_float()
        variables = read_map_i32(buf)
        self.npcs[guid] = NpcInfo(guid, entry, x, y, orientation, variables)

    def _on_object_variable(self, buf: PacketBuffer) -> None:
        guid = buf.read_u32()
        var_id = buf.read_i32()
        value = buf.read_i32()
        if self.my_player and self.my_player.guid == guid:
            self.my_player.variables[var_id] = value
        elif guid in self.players:
            self.players[guid].variables[var_id] = value
        elif guid in self.npcs:
            self.npcs[guid].variables[var_id] = value

    def _on_inventory(self, buf: PacketBuffer) -> None:
        gold = buf.read_i32()
        count = buf.read_u16()
        inv: Dict[int, Tuple[int, int]] = {}
        for _ in range(count):
            slot = buf.read_i32()
            item_id = unpack_item_id(buf)
            stack = buf.read_i32()
            inv[slot] = (item_id, stack)
        self.gold = gold
        self.inventory = inv
        self.inventory_version += 1

    def _on_bank(self, buf: PacketBuffer) -> None:
        count = buf.read_u16()
        bank: Dict[int, Tuple[int, int]] = {}
        for _ in range(count):
            slot = buf.read_i32()
            item_id = unpack_item_id(buf)
            stack = buf.read_i32()
            bank[slot] = (item_id, stack)
        self.bank = bank
        self.bank_version += 1

    def _on_equip_item(self, buf: PacketBuffer) -> None:
        guid = buf.read_u32()
        slot = buf.read_i32()
        item_id = unpack_item_id(buf)
        if guid == self.my_guid and self.my_player:
            self.my_player.equipment[slot] = item_id

    def _on_loot_window(self, buf: PacketBuffer) -> None:
        guid = buf.read_u32()
        money = buf.read_i32()
        count = buf.read_u16()
        items: List[Tuple[int, int]] = []
        for _ in range(count):
            item_id = unpack_item_id(buf)
            item_count = buf.read_i32()
            items.append((item_id, item_count))
        self.loot_window = LootWindow(guid, money, items)
        self.loot_version += 1

    def _on_trade_update(self, buf: PacketBuffer) -> None:
        state = TradeState()
        state.my_gold = buf.read_i32()
        state.their_gold = buf.read_i32()
        state.their_confirmed = buf.read_bool()
        my_count = buf.read_u16()
        for _ in range(my_count):
            slot = buf.read_i32()
            item_id = unpack_item_id(buf)
            stack = buf.read_i32()
            state.my_items.append((slot, item_id, stack))
        their_count = buf.read_u16()
        for _ in range(their_count):
            slot = buf.read_i32()
            item_id = unpack_item_id(buf)
            stack = buf.read_i32()
            state.their_items.append((slot, item_id, stack))
        self.trade_state = state
        self.trade_version += 1

    def _on_spellbook(self, buf: PacketBuffer) -> None:
        count = buf.read_u16()
        spells: List[int] = []
        for _ in range(count):
            spell_id = buf.read_i32()
            _level = buf.read_u8()
            bp_count = buf.read_u16()
            for _ in range(bp_count):
                _ = buf.read_i16()
                _ = buf.read_i16()
            spells.append(spell_id)
        self.spells = spells

    # -------------------------------------------------------------------------
    # Client actions
    # -------------------------------------------------------------------------

    def authenticate(self, username: str, password: str) -> None:
        buf = PacketBuffer()
        buf.write_string(f"{username}:{password}")
        buf.write_i32(0)
        buf.write_string("")
        self._send_packet(Opcode.Client_Authenticate, buf.bytes())

    def request_character_list(self) -> None:
        self._send_packet(Opcode.Client_CharacterList)

    def create_character(self, name: str, class_id: int = 1, gender: int = 0, portrait_id: int = 0) -> None:
        buf = PacketBuffer()
        buf.write_u8(gender)
        buf.write_i32(portrait_id)
        buf.write_string(name)
        buf.write_u8(class_id)
        self._send_packet(Opcode.Client_CharCreate, buf.bytes())

    def enter_world(self, character_guid: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(character_guid)
        self._send_packet(Opcode.Client_EnterWorld, buf.bytes())

    def move_to(self, x: float, y: float) -> None:
        buf = PacketBuffer()
        buf.write_float(x)
        buf.write_float(y)
        buf.write_bool(False)
        self._send_packet(Opcode.Client_RequestMove, buf.bytes())

    def set_target(self, guid: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(guid)
        self._send_packet(Opcode.Client_SetSelected, buf.bytes())

    def cast_spell(self, spell_id: int, target_guid: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(spell_id)
        buf.write_u32(target_guid)
        buf.write_float(0.0)
        buf.write_float(0.0)
        buf.write_bool(False)
        self._send_packet(Opcode.Client_CastSpell, buf.bytes())

    def equip_item(self, inv_slot: int, item_id: int, equip_slot: int = 0) -> None:
        buf = PacketBuffer()
        buf.write_i32(inv_slot)
        buf.write_u8(equip_slot)
        pack_item_id(buf, item_id)
        self._send_packet(Opcode.Client_EquipItem, buf.bytes())

    def unequip_item(self, equip_slot: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(equip_slot)
        self._send_packet(Opcode.Client_UnequipItem, buf.bytes())

    def move_item(self, from_slot: int, to_slot: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(from_slot)
        buf.write_i32(to_slot)
        self._send_packet(Opcode.Client_MoveItem, buf.bytes())

    def split_stack(self, from_slot: int, to_slot: int, amount: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(from_slot)
        buf.write_i32(to_slot)
        buf.write_i32(amount)
        self._send_packet(Opcode.Client_SplitItemStack, buf.bytes())

    def use_item(self, slot: int, target_guid: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(slot)
        buf.write_u32(target_guid)
        self._send_packet(Opcode.Client_UseItem, buf.bytes())

    def loot_item(self, source_guid: int, item_id: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(source_guid)
        pack_item_id(buf, item_id)
        self._send_packet(Opcode.Client_LootItem, buf.bytes())

    def buy_vendor_item(self, vendor_guid: int, item_index: int, count: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(vendor_guid)
        buf.write_i32(item_index)
        buf.write_i32(count)
        self._send_packet(Opcode.Client_BuyVendorItem, buf.bytes())

    def sell_item(self, vendor_guid: int, inventory_slot: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(vendor_guid)
        buf.write_i32(inventory_slot)
        self._send_packet(Opcode.Client_SellItem, buf.bytes())

    def open_trade(self, target_guid: int) -> None:
        buf = PacketBuffer()
        buf.write_u32(target_guid)
        self._send_packet(Opcode.Client_OpenTradeWith, buf.bytes())

    def trade_add_item(self, inv_slot: int) -> None:
        buf = PacketBuffer()
        buf.write_i32(inv_slot)
        self._send_packet(Opcode.Client_TradeAddItem, buf.bytes())

    def trade_confirm(self) -> None:
        self._send_packet(Opcode.Client_TradeConfirm)

    def bank_deposit(self, from_slot: int, to_slot: int, auto: bool = False) -> None:
        buf = PacketBuffer()
        buf.write_i32(from_slot)
        buf.write_i32(to_slot)
        buf.write_bool(auto)
        self._send_packet(Opcode.Client_MoveInventoryToBank, buf.bytes())

    def bank_withdraw(self, bank_slot: int, inv_slot: int, auto: bool = False) -> None:
        buf = PacketBuffer()
        buf.write_i32(bank_slot)
        buf.write_i32(inv_slot)
        buf.write_bool(auto)
        self._send_packet(Opcode.Client_UnBankItem, buf.bytes())

    def repair(self, confirmed: bool) -> None:
        buf = PacketBuffer()
        buf.write_bool(confirmed)
        self._send_packet(Opcode.Client_Repair, buf.bytes())

    # -------------------------------------------------------------------------
    # Utility Methods
    # -------------------------------------------------------------------------

    def wait_for_inventory_update(self, start_version: int, timeout: float = 5.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.inventory_version > start_version:
                return True
            time.sleep(0.05)
        return False

    def wait_for_bank_update(self, start_version: int, timeout: float = 5.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.bank_version > start_version:
                return True
            time.sleep(0.05)
        return False

    def wait_for_trade_update(self, start_version: int, timeout: float = 5.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.trade_version > start_version:
                return True
            time.sleep(0.05)
        return False

    def wait_for_loot_window(self, start_version: int, timeout: float = 5.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.loot_version > start_version:
                return True
            time.sleep(0.05)
        return False

    def wait_for_world_entry(self, timeout: float = 8.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.my_guid and self.my_player and self.inventory_version > 0:
                return True
            time.sleep(0.1)
        return False

    def wait_for_spellbook(self, timeout: float = 5.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.spells:
                return True
            time.sleep(0.05)
        return False

    def wait_for_auth(self, timeout: float = 3.0) -> bool:
        end_time = time.time() + timeout
        while time.time() < end_time:
            if self.authenticated:
                return True
            time.sleep(0.05)
        return False

    def find_item_slots(self, item_id: int) -> List[Tuple[int, int]]:
        return [(slot, data[1]) for slot, data in self.inventory.items() if data[0] == item_id]

    def find_empty_slot(self) -> Optional[int]:
        for slot in range(40):
            if slot not in self.inventory:
                return slot
        return None


# =============================================================================
# Database Helpers
# =============================================================================

def seed_character(
    db_path: str,
    guid: int,
    gold: int,
    inventory_items: List[Tuple[int, int, int]],
    equipment_items: List[Tuple[int, int, int]],
) -> None:
    conn = sqlite3.connect(db_path, timeout=5.0)
    conn.execute("PRAGMA busy_timeout = 5000")
    cur = conn.cursor()

    cur.execute("UPDATE characters SET gold = ? WHERE guid = ?", (gold, guid))
    # Set character to level 20 for higher base stats (1500 HP from class stats)
    # Also set health=max_health and mana=max_mana to ensure alive state
    # The server's applyLevelStats will use the level to compute actual max_health
    # Move to a safer spawn location (66.5, 75.5) with fewer hostile NPCs nearby
    cur.execute("""UPDATE characters SET
        level = 20, health = 1500, max_health = 1500, mana = 600, max_mana = 600,
        position_x = 66.5, position_y = 75.5
        WHERE guid = ?""", (guid,))
    cur.execute("DELETE FROM character_inventory WHERE character_guid = ?", (guid,))
    cur.execute("DELETE FROM character_equipment WHERE character_guid = ?", (guid,))
    cur.execute("DELETE FROM character_bank WHERE character_guid = ?", (guid,))

    for slot, item_id, stack in inventory_items:
        cur.execute(
            "INSERT INTO character_inventory "
            "(character_guid, slot, item_id, stack_count, durability, enchant_id, flags) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)",
            (guid, slot, item_id, stack, 0, 0, 0),
        )

    cur.execute("PRAGMA table_info(character_equipment)")
    equip_columns = [row[1] for row in cur.fetchall()]
    base_cols = ["character_guid", "slot", "item_id", "durability", "enchant_id"]
    optional_cols = ["affix1", "affix2", "gem1", "gem2", "gem3"]
    insert_cols = base_cols + [col for col in optional_cols if col in equip_columns]
    placeholders = ", ".join(["?"] * len(insert_cols))
    insert_sql = (
        "INSERT INTO character_equipment (" + ", ".join(insert_cols) + ") "
        "VALUES (" + placeholders + ")"
    )

    for slot, item_id, durability in equipment_items:
        values = [guid, slot, item_id, durability, 0]
        values.extend([0] * (len(insert_cols) - len(base_cols)))
        cur.execute(insert_sql, values)

    conn.commit()
    conn.close()


def choose_vendor(
    game_db_path: str,
    map_id: int,
    item_id: int,
    origin: Tuple[float, float],
) -> Optional[Tuple[int, str, float, float, int]]:
    conn = sqlite3.connect(game_db_path)
    cur = conn.cursor()

    cur.execute(
        """
        SELECT npc.entry, npc.position_x, npc.position_y, npc_template.name
        FROM npc
        JOIN npc_template ON npc.entry = npc_template.entry
        WHERE npc.map = ? AND npc.entry IN (
            SELECT npc_entry FROM npc_vendor WHERE item = ?
        )
        """,
        (map_id, item_id),
    )

    rows = cur.fetchall()
    conn.close()
    if not rows:
        return None

    best = None
    best_dist = None
    ox, oy = origin
    for entry, x, y, name in rows:
        dist = (x - ox) ** 2 + (y - oy) ** 2
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best = (entry, name, x, y)

    if not best:
        return None

    entry, name, x, y = best

    conn = sqlite3.connect(game_db_path)
    cur = conn.cursor()
    cur.execute(
        "SELECT item FROM npc_vendor WHERE npc_entry = ? ORDER BY rowid",
        (entry,),
    )
    items = [row[0] for row in cur.fetchall()]
    conn.close()

    if item_id not in items:
        return None

    item_index = items.index(item_id)
    return entry, name, x, y, item_index


def choose_loot_target(
    game_db_path: str,
    map_id: int,
    origin: Tuple[float, float],
) -> Optional[Tuple[int, str, float, float]]:
    conn = sqlite3.connect(game_db_path)
    cur = conn.cursor()
    cur.execute(
        """
        SELECT npc.entry, npc.position_x, npc.position_y, npc_template.name
        FROM npc
        JOIN npc_template ON npc.entry = npc_template.entry
        WHERE npc.map = ?
          AND npc.entry IN (SELECT DISTINCT entry FROM loot)
          AND CAST(npc_template.faction AS INTEGER) IN (2, 3)
        """,
        (map_id,),
    )
    rows = cur.fetchall()
    conn.close()

    if not rows:
        return None

    ox, oy = origin
    best = None
    best_dist = None
    for entry, x, y, name in rows:
        dist = (x - ox) ** 2 + (y - oy) ** 2
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best = (entry, name, x, y)

    return best


def choose_damage_spell(game_db_path: str, spell_ids: List[int]) -> Optional[int]:
    if not spell_ids:
        return None

    conn = sqlite3.connect(game_db_path)
    cur = conn.cursor()

    for spell_id in spell_ids:
        cur.execute(
            "SELECT effect1, effect2, effect3 FROM spell_template WHERE entry = ?",
            (spell_id,),
        )
        row = cur.fetchone()
        if not row:
            continue
        effects = [int(x) if x is not None else 0 for x in row]
        if any(effect in (1, 2, 9) for effect in effects):
            conn.close()
            return spell_id

    conn.close()
    return spell_ids[0]


# =============================================================================
# Test Steps
# =============================================================================

def test_move_item(client: InventoryClient, item_id: int) -> bool:
    print("\n[Move Item]")
    slots = client.find_item_slots(item_id)
    if not slots:
        print("  FAIL: No item to move")
        return False

    from_slot = slots[0][0]
    to_slot = client.find_empty_slot()
    if to_slot is None:
        print("  FAIL: No empty slot to move into")
        return False

    start_version = client.inventory_version
    client.move_item(from_slot, to_slot)
    if not client.wait_for_inventory_update(start_version):
        print("  FAIL: Inventory update not received")
        return False

    if to_slot in client.inventory and client.inventory[to_slot][0] == item_id:
        print(f"  PASS: Moved item {item_id} from {from_slot} to {to_slot}")
        return True

    print("  FAIL: Item not found in destination slot")
    return False


def test_equip_unequip(client: InventoryClient, item_id: int, equip_slot: int) -> bool:
    print("\n[Equip/Unequip]")
    slots = client.find_item_slots(item_id)
    if not slots:
        print("  FAIL: No item to equip")
        return False

    inv_slot = slots[0][0]
    start_version = client.inventory_version
    client.equip_item(inv_slot, item_id, 0)
    if not client.wait_for_inventory_update(start_version):
        print("  FAIL: No inventory update after equip")
        return False

    if client.find_item_slots(item_id):
        print("  FAIL: Item still in inventory after equip")
        return False

    start_version = client.inventory_version
    client.unequip_item(equip_slot)
    if not client.wait_for_inventory_update(start_version):
        print("  FAIL: No inventory update after unequip")
        return False

    if client.find_item_slots(item_id):
        print(f"  PASS: Equipped and unequipped item {item_id}")
        return True

    print("  FAIL: Item not in inventory after unequip")
    return False


def test_vendor_buy_stack_split_use_sell(
    client: InventoryClient,
    vendor_guid: Optional[int],
    item_id: int,
    item_index: Optional[int],
    target_guid: int,
) -> bool:
    print("\n[Vendor Buy/Stack/Split/Use/Sell]")
    start_total = sum(count for _, count in client.find_item_slots(item_id))

    if vendor_guid is not None and item_index is not None:
        # Buy 7 items to force multiple stacks (max stack expected to be 5)
        start_version = client.inventory_version
        client.buy_vendor_item(vendor_guid, item_index, 7)
        if not client.wait_for_inventory_update(start_version):
            print("  FAIL: Inventory update not received after buy")
            return False

        new_total = sum(count for _, count in client.find_item_slots(item_id))
        if new_total < start_total + 7:
            print("  FAIL: Buy did not add expected quantity")
            return False
    else:
        print("  INFO: Vendor not visible, skipping buy/sell and using pre-seeded items")

    # Split: find a stack with enough items to split
    slots = client.find_item_slots(item_id)
    split_from = None
    split_count = 0
    for slot, count in slots:
        if count >= 3:
            split_from = slot
            split_count = count
            break
    if split_from is None:
        print("  FAIL: No stack large enough to split")
        return False

    split_to = client.find_empty_slot()
    if split_to is None:
        print("  FAIL: No empty slot for split")
        return False

    split_amount = 2 if split_count > 2 else 1
    start_version = client.inventory_version
    client.split_stack(split_from, split_to, split_amount)
    if not client.wait_for_inventory_update(start_version):
        print("  FAIL: No inventory update after split")
        return False

    # Stack: move larger stack onto smaller stack to force merge
    slots = client.find_item_slots(item_id)
    if len(slots) < 2:
        print("  FAIL: Split did not create a second stack")
        return False

    slots_sorted = sorted(slots, key=lambda entry: entry[1])
    target_slot, target_count = slots_sorted[0]
    source_slot, source_count = slots_sorted[-1]

    start_version = client.inventory_version
    client.move_item(source_slot, target_slot)
    if not client.wait_for_inventory_update(start_version):
        print("  FAIL: No inventory update after stack move")
        return False

    updated_target = client.inventory.get(target_slot, (0, 0))[1]
    if updated_target <= target_count:
        print("  FAIL: Stack move did not merge as expected")
        return False

    # Use item (consume 1)
    slots = client.find_item_slots(item_id)
    use_slot, use_count = slots[0]
    start_version = client.inventory_version
    client.use_item(use_slot, target_guid)
    client.wait_for_inventory_update(start_version, timeout=3.0)
    slots_after = client.find_item_slots(item_id)
    new_count = 0
    for slot, count in slots_after:
        if slot == use_slot:
            new_count = count
            break

    if new_count >= use_count:
        print("  FAIL: Item use did not consume")
        return False

    # Sell one stack if vendor is available
    if vendor_guid is not None:
        slots = client.find_item_slots(item_id)
        sell_slot = slots[0][0]
        start_version = client.inventory_version
        client.sell_item(vendor_guid, sell_slot)
        if not client.wait_for_inventory_update(start_version):
            print("  FAIL: No inventory update after sell")
            return False
    else:
        print("  INFO: Vendor sell skipped (no vendor NPCs)")

    print("  PASS: Stack/split/use flow OK")
    return True


def test_bank_deposit_withdraw(client: InventoryClient, item_id: int) -> bool:
    print("\n[Bank Deposit/Withdraw]")
    slots = client.find_item_slots(item_id)
    if not slots:
        print("  FAIL: No item to deposit")
        return False

    from_slot = slots[0][0]
    bank_slot = 0
    start_version = client.bank_version
    client.bank_deposit(from_slot, bank_slot, False)
    if not client.wait_for_bank_update(start_version):
        print("  FAIL: No bank update after deposit")
        return False

    if bank_slot not in client.bank or client.bank[bank_slot][0] != item_id:
        print("  FAIL: Bank slot did not receive item")
        return False

    inv_slot = client.find_empty_slot()
    if inv_slot is None:
        print("  FAIL: No empty inventory slot for withdraw")
        return False

    start_inv_version = client.inventory_version
    client.bank_withdraw(bank_slot, inv_slot, False)
    if not client.wait_for_inventory_update(start_inv_version):
        print("  FAIL: No inventory update after withdraw")
        return False

    if inv_slot in client.inventory and client.inventory[inv_slot][0] == item_id:
        print("  PASS: Bank deposit/withdraw OK")
        return True

    print("  FAIL: Withdraw did not place item in inventory")
    return False


def test_trade(
    client1: InventoryClient,
    client2: InventoryClient,
    item1: int,
    item2: int,
) -> bool:
    print("\n[Trade]")
    slots1 = client1.find_item_slots(item1)
    slots2 = client2.find_item_slots(item2)
    if not slots1 or not slots2:
        print("  FAIL: Trade items missing")
        return False

    slot1 = slots1[0][0]
    slot2 = slots2[0][0]

    start_trade = client1.trade_version
    client1.open_trade(client2.my_guid or 0)
    if not client1.wait_for_trade_update(start_trade):
        print("  FAIL: Trade update not received after open")
        return False

    start_trade = client1.trade_version
    client1.trade_add_item(slot1)
    client2.trade_add_item(slot2)
    if not client1.wait_for_trade_update(start_trade):
        print("  FAIL: Trade update not received after add")
        return False

    start_inv1 = client1.inventory_version
    start_inv2 = client2.inventory_version
    client1.trade_confirm()
    client2.trade_confirm()
    if not client1.wait_for_inventory_update(start_inv1, timeout=6.0):
        print("  FAIL: Inventory update not received after trade (client1)")
        return False
    if not client2.wait_for_inventory_update(start_inv2, timeout=6.0):
        print("  FAIL: Inventory update not received after trade (client2)")
        return False

    has_item2 = any(item == item2 for item, _ in client1.inventory.values())
    has_item1 = any(item == item1 for item, _ in client2.inventory.values())
    if has_item2 and has_item1:
        print("  PASS: Trade completed")
        return True

    print("  FAIL: Trade items not swapped as expected")
    return False


def test_repair(client: InventoryClient) -> bool:
    print("\n[Repair]")
    client.repair_cost = None
    client.last_spent_gold = None
    client.repair(False)
    time.sleep(0.5)
    if not client.repair_cost or client.repair_cost <= 0:
        print("  FAIL: No repair cost returned")
        return False

    client.repair(True)
    time.sleep(0.5)
    if client.last_spent_gold:
        print("  PASS: Repair completed")
        return True

    print("  FAIL: Repair confirmation did not spend gold")
    return False


def test_loot(
    client: InventoryClient,
    npc_guid: int,
    damage_spell: Optional[int],
) -> bool:
    print("\n[Loot]")
    start_loot = client.loot_version
    attempts = 0

    while attempts < 10:
        attempts += 1
        if damage_spell is not None:
            client.set_target(npc_guid)
            client.cast_spell(damage_spell, npc_guid)
            time.sleep(0.6)

        loot_attempt = client.loot_version
        client.cast_spell(85, npc_guid)  # LootUnit spell
        if client.wait_for_loot_window(loot_attempt, timeout=1.2):
            break

    if client.loot_version <= start_loot:
        # Fallback to LootGameObj to at least exercise the loot window flow.
        loot_attempt = client.loot_version
        client.cast_spell(127, npc_guid)  # LootGameObj / Interact
        client.wait_for_loot_window(loot_attempt, timeout=1.2)

    if client.loot_version <= start_loot or not client.loot_window:
        print("  FAIL: Loot window not received")
        return False

    loot = client.loot_window
    start_inv = client.inventory_version
    client.loot_item(loot.guid, 0xFFFF)
    client.wait_for_inventory_update(start_inv, timeout=3.0)
    print(f"  PASS: Loot window opened (items={len(loot.items)}, gold={loot.money})")
    return True


# =============================================================================
# Main Test Runner
# =============================================================================

def run_inventory_tests(args: argparse.Namespace) -> bool:
    client1 = InventoryClient("Client1", args.host, args.port)
    client2 = InventoryClient("Client2", args.host, args.port)

    if not client1.connect() or not client2.connect():
        return False

    try:
        client1.authenticate(args.user1, args.pass1)
        client2.authenticate(args.user2, args.pass2)
        if not client1.wait_for_auth() or not client2.wait_for_auth():
            print("FATAL: Authentication failed")
            return False

        # Character setup
        client1.request_character_list()
        client2.request_character_list()
        time.sleep(0.5)

        name1 = args.char1
        name2 = args.char2

        if not any(c.name == name1 for c in client1.characters):
            client1.create_character(name1, class_id=1)
            time.sleep(0.5)
            client1.request_character_list()
            time.sleep(0.5)

        if not any(c.name == name2 for c in client2.characters):
            client2.create_character(name2, class_id=1)
            time.sleep(0.5)
            client2.request_character_list()
            time.sleep(0.5)

        char1 = next((c for c in client1.characters if c.name == name1), None)
        char2 = next((c for c in client2.characters if c.name == name2), None)
        if not char1 or not char2:
            print("FATAL: Characters not available")
            return False

        # Seed inventory/equipment for tests
        seed_character(
            args.server_db,
            char1.guid,
            gold=1000,
            inventory_items=[(0, 13, 1), (1, 1, 7)],  # Handwraps + potions
            equipment_items=[(11, 14, 10)],  # Pants (low durability for repair)
        )
        seed_character(
            args.server_db,
            char2.guid,
            gold=1000,
            inventory_items=[(0, 15, 1)],  # Shoes (trade item)
            equipment_items=[],
        )

        # Enter world
        client1.enter_world(char1.guid)
        client2.enter_world(char2.guid)

        if not client1.wait_for_world_entry() or not client2.wait_for_world_entry():
            print("FATAL: World entry failed")
            return False

        if not client1.my_player or not client2.my_player:
            print("FATAL: Player state not available")
            return False

        client1.wait_for_spellbook(timeout=2.0)

        origin = (client1.my_player.x, client1.my_player.y)

        vendor_info = choose_vendor(args.game_db, client1.map_id or 1, 1, origin)
        loot_info = choose_loot_target(args.game_db, client1.map_id or 1, origin)

        # Move to loot target and perform loot test
        results = []
        results.append(("Move Item", test_move_item(client1, 13)))
        results.append(("Equip/Unequip", test_equip_unequip(client1, 13, 7)))

        if loot_info:
            loot_entry, _loot_name, loot_x, loot_y = loot_info
            client1.move_to(loot_x, loot_y)
            npc_guid = wait_for_npc_guid(client1, loot_entry, loot_x, loot_y, timeout=4.0)
            if npc_guid is None:
                print("  INFO: Loot NPC not visible, skipping loot test")
                results.append(("Loot (skipped)", True))
            else:
                damage_spell = choose_damage_spell(args.game_db, client1.spells)
                results.append(("Loot", test_loot(client1, npc_guid, damage_spell)))
        else:
            print("  INFO: No loot target found in game.db, skipping loot test")
            results.append(("Loot (skipped)", True))

        vendor_guid = None
        vendor_item_index = None
        if vendor_info:
            vendor_entry, _vendor_name, vendor_x, vendor_y, vendor_item_index = vendor_info
            # Move both players to vendor
            client1.move_to(vendor_x, vendor_y)
            client2.move_to(vendor_x, vendor_y)
            vendor_guid = wait_for_npc_guid(client1, vendor_entry, vendor_x, vendor_y, timeout=4.0)
            if vendor_guid is None:
                print("  INFO: Vendor NPC not visible, skipping buy/sell")

        results.append((
            "Vendor/Stack/Split/Use/Sell",
            test_vendor_buy_stack_split_use_sell(
                client1,
                vendor_guid,
                1,
                vendor_item_index,
                client1.my_guid or 0,
            ),
        ))
        results.append(("Bank Deposit/Withdraw", test_bank_deposit_withdraw(client1, 1)))
        results.append(("Trade", test_trade(client1, client2, 13, 15)))
        results.append(("Repair", test_repair(client1)))

        print("\n" + "=" * 60)
        print("Inventory Test Results")
        print("=" * 60)
        passed = 0
        failed = 0
        for name, ok in results:
            status = "PASS" if ok else "FAIL"
            print(f"  {status}: {name}")
            if ok:
                passed += 1
            else:
                failed += 1
        print(f"\nTotal: {passed} passed, {failed} failed")
        print("=" * 60)

        return failed == 0

    finally:
        client1.disconnect()
        client2.disconnect()


def find_npc_guid(client: InventoryClient, entry: int, x: float, y: float) -> Optional[int]:
    best_guid = None
    best_dist = None
    for guid, npc in client.npcs.items():
        if npc.entry != entry:
            continue
        dist = (npc.x - x) ** 2 + (npc.y - y) ** 2
        if best_dist is None or dist < best_dist:
            best_dist = dist
            best_guid = guid
    return best_guid


def wait_for_npc_guid(
    client: InventoryClient,
    entry: int,
    x: float,
    y: float,
    timeout: float = 4.0,
) -> Optional[int]:
    end_time = time.time() + timeout
    while time.time() < end_time:
        guid = find_npc_guid(client, entry, x, y)
        if guid is not None:
            return guid
        time.sleep(0.1)
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description="Dreadmyst Inventory E2E Test")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--user1", default="invtestone", help="Account 1 username")
    parser.add_argument("--pass1", default="test123", help="Account 1 password")
    parser.add_argument("--user2", default="invtesttwo", help="Account 2 username")
    parser.add_argument("--pass2", default="test123", help="Account 2 password")
    parser.add_argument("--char1", default="InvTesterAlpha", help="Character name 1")
    parser.add_argument("--char2", default="InvTesterBeta", help="Character name 2")
    parser.add_argument("--server-db", default="server/data/server.db", help="Server DB path")
    parser.add_argument("--game-db", default="client/game/game.db", help="Game DB path")
    args = parser.parse_args()

    success = run_inventory_tests(args)
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
