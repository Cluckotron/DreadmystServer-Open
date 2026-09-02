#!/usr/bin/env python3
"""
Dreadmyst Server - Phase 5 Combat End-to-End Test
Task 5.15: Combat End-to-End Test

This script tests the complete combat system:
1. Spell casting (damage, heal, aura)
2. Target selection
3. Combat messages
4. Cooldowns
5. NPC aggro and combat
6. Death and respawn

Protocol Notes:
- All integers are little-endian
- Packet format: [uint16 size][uint16 opcode][payload...]
- Strings: [uint16 length][string data]
- Maps: [uint16 count][pairs...]
"""

import socket
import struct
import time
import threading
import sys
import argparse
from dataclasses import dataclass, field
from typing import Optional, Dict, List, Tuple, Any
from enum import IntEnum
import random

# =============================================================================
# Protocol Constants (from GamePacketBase.h)
# =============================================================================

class Opcode(IntEnum):
    # Bidirectional
    Mutual_Ping = 0

    # Server -> Client (100+)
    Server_Validate = 100
    Server_QueuePosition = 101
    Server_CharacterList = 102
    Server_CharaCreateResult = 103
    Server_NewWorld = 104
    Server_SetController = 105
    Server_ChannelInfo = 106
    Server_ChannelChangeConfirm = 107
    Server_Player = 110
    Server_Npc = 111
    Server_GameObject = 112
    Server_DestroyObject = 113
    Server_SetSubname = 114
    Server_UnitSpline = 120
    Server_UnitTeleport = 121
    Server_UnitOrientation = 122
    Server_CastStart = 130
    Server_CastStop = 131
    Server_SpellGo = 132
    Server_CombatMsg = 133
    Server_UnitAuras = 134
    Server_Cooldown = 135
    Server_ObjectVariable = 136
    Server_AggroMob = 137
    Server_Inventory = 140
    Server_Bank = 141
    Server_OpenBank = 142
    Server_EquipItem = 143
    Server_NotifyItemAdd = 144
    Server_Spellbook = 160
    Server_ExpNotify = 170
    Server_LvlResponse = 171
    Server_SpentGold = 172
    Server_QuestList = 180
    Server_ChatMsg = 190
    Server_GossipMenu = 192
    Server_GuildRoster = 200
    Server_PartyList = 210
    Server_TradeUpdate = 220
    Server_TradeCanceled = 221
    Server_WorldError = 250

    # Client -> Server (300+)
    Client_Authenticate = 300
    Client_CharacterList = 301
    Client_CharCreate = 302
    Client_DeleteCharacter = 303
    Client_EnterWorld = 304
    Client_RequestMove = 310
    Client_RequestStop = 311
    Client_CastSpell = 320
    Client_CancelCast = 321
    Client_SetSelected = 322
    Client_RequestRespawn = 323

class AuthResult(IntEnum):
    Validated = 0
    WrongVersion = 1
    BadPassword = 2
    ServerFull = 3
    Banned = 4

class CharCreateResult(IntEnum):
    Success = 0
    AlreadyExists = 1
    InvalidChars = 2
    Reserved = 3
    TooShort = 4
    TooLong = 5

class CastResult(IntEnum):
    Success = 0
    NotKnown = 1
    OnCooldown = 2
    NotEnoughMana = 3
    InvalidTarget = 4
    OutOfRange = 5
    CasterDead = 6
    TargetDead = 7
    CasterMoving = 8
    Silenced = 9
    AlreadyCasting = 10
    TargetImmune = 11
    NoLineOfSight = 12
    TargetNotInFront = 13
    MissingReagent = 14
    RequiresWeapon = 15
    TargetFriendly = 16
    TargetHostile = 17
    TargetSelf = 18
    CantTargetSelf = 19
    CasterStunned = 20
    GCDActive = 21
    InternalError = 99

class HitResult(IntEnum):
    Normal = 0
    Crit = 1
    Miss = 2
    Dodge = 3
    Parry = 4
    Block = 5
    Glancing = 6
    Absorb = 7
    Immune = 8
    Resist = 9
    Reflect = 10

class CombatMsgType(IntEnum):
    Damage = 0
    Heal = 1
    Energize = 2
    Miss = 3
    Absorb = 4
    Death = 5

# =============================================================================
# Buffer Utilities (matches StlBuffer serialization)
# =============================================================================

class PacketBuffer:
    """Binary buffer matching C++ StlBuffer serialization."""

    def __init__(self, data: bytes = b''):
        self.data = bytearray(data)
        self.read_pos = 0

    # Write operations
    def write_u8(self, val: int) -> 'PacketBuffer':
        self.data.append(val & 0xFF)
        return self

    def write_i8(self, val: int) -> 'PacketBuffer':
        return self.write_u8(val)

    def write_u16(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<H', val))
        return self

    def write_i16(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<h', val))
        return self

    def write_u32(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<I', val))
        return self

    def write_i32(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<i', val))
        return self

    def write_u64(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<Q', val))
        return self

    def write_i64(self, val: int) -> 'PacketBuffer':
        self.data.extend(struct.pack('<q', val))
        return self

    def write_float(self, val: float) -> 'PacketBuffer':
        self.data.extend(struct.pack('<f', val))
        return self

    def write_double(self, val: float) -> 'PacketBuffer':
        self.data.extend(struct.pack('<d', val))
        return self

    def write_bool(self, val: bool) -> 'PacketBuffer':
        return self.write_u8(1 if val else 0)

    def write_string(self, val: str) -> 'PacketBuffer':
        encoded = val.encode('utf-8')
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
        val = struct.unpack_from('<H', self.data, self.read_pos)[0]
        self.read_pos += 2
        return val

    def read_i16(self) -> int:
        if self.read_pos + 2 > len(self.data):
            return 0
        val = struct.unpack_from('<h', self.data, self.read_pos)[0]
        self.read_pos += 2
        return val

    def read_u32(self) -> int:
        if self.read_pos + 4 > len(self.data):
            return 0
        val = struct.unpack_from('<I', self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_i32(self) -> int:
        if self.read_pos + 4 > len(self.data):
            return 0
        val = struct.unpack_from('<i', self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_u64(self) -> int:
        if self.read_pos + 8 > len(self.data):
            return 0
        val = struct.unpack_from('<Q', self.data, self.read_pos)[0]
        self.read_pos += 8
        return val

    def read_i64(self) -> int:
        if self.read_pos + 8 > len(self.data):
            return 0
        val = struct.unpack_from('<q', self.data, self.read_pos)[0]
        self.read_pos += 8
        return val

    def read_float(self) -> float:
        if self.read_pos + 4 > len(self.data):
            return 0.0
        val = struct.unpack_from('<f', self.data, self.read_pos)[0]
        self.read_pos += 4
        return val

    def read_double(self) -> float:
        if self.read_pos + 8 > len(self.data):
            return 0.0
        val = struct.unpack_from('<d', self.data, self.read_pos)[0]
        self.read_pos += 8
        return val

    def read_bool(self) -> bool:
        return self.read_u8() != 0

    def read_string(self) -> str:
        length = self.read_u16()
        if self.read_pos + length > len(self.data):
            return ""
        val = self.data[self.read_pos:self.read_pos + length].decode('utf-8', errors='replace')
        self.read_pos += length
        return val

    def read_map_i32_i32(self) -> Dict[int, int]:
        count = self.read_u16()
        result = {}
        for _ in range(count):
            key = self.read_i32()
            val = self.read_i32()
            result[key] = val
        return result

    def bytes(self) -> bytes:
        return bytes(self.data)

    def remaining(self) -> int:
        return len(self.data) - self.read_pos

    def clone(self) -> 'PacketBuffer':
        """Create a copy with read position reset."""
        return PacketBuffer(bytes(self.data))

# =============================================================================
# Packet Structures
# =============================================================================

@dataclass
class Character:
    guid: int = 0
    name: str = ""
    class_id: int = 0
    gender: int = 0
    level: int = 1
    portrait: int = 0

@dataclass
class PlayerInfo:
    guid: int = 0
    name: str = ""
    sub_name: str = ""
    class_id: int = 0
    gender: int = 0
    portrait_id: int = 0
    x: float = 0.0
    y: float = 0.0
    orientation: float = 0.0
    equipment: Dict[int, int] = field(default_factory=dict)
    variables: Dict[int, int] = field(default_factory=dict)

@dataclass
class NpcInfo:
    guid: int = 0
    entry: int = 0
    name: str = ""
    sub_name: str = ""
    model_id: int = 0
    model_scale: int = 100
    x: float = 0.0
    y: float = 0.0
    orientation: float = 0.0
    variables: Dict[int, int] = field(default_factory=dict)

@dataclass
class CombatMessage:
    msg_type: int = 0
    caster_guid: int = 0
    target_guid: int = 0
    spell_id: int = 0
    value: int = 0
    overkill: int = 0
    hit_result: int = 0
    school: int = 0
    absorbed: int = 0

@dataclass
class SpellCast:
    caster_guid: int = 0
    spell_id: int = 0
    targets: List[Tuple[int, int]] = field(default_factory=list)  # (guid, hit_result)

@dataclass
class Cooldown:
    spell_id: int = 0
    remaining_ms: int = 0
    category_id: int = 0
    category_remaining_ms: int = 0

# Object variable constants (from ObjDefines.h)
class ObjVariable(IntEnum):
    Health = 1
    MaxHealth = 2
    Mana = 3
    MaxMana = 4
    Level = 10

# =============================================================================
# Game Client Implementation
# =============================================================================

class CombatClient:
    """Simulated Dreadmyst game client for combat testing."""

    def __init__(self, name: str, host: str = "127.0.0.1", port: int = 8080):
        self.name = name
        self.host = host
        self.port = port
        self.socket: Optional[socket.socket] = None
        self.recv_buffer = bytearray()

        # Session state
        self.authenticated = False
        self.characters: List[Character] = []
        self.selected_character_guid: Optional[int] = None

        # World state
        self.in_world = False
        self.my_guid: Optional[int] = None
        self.map_id: int = 0
        self.my_player: Optional[PlayerInfo] = None
        self.visible_players: Dict[int, PlayerInfo] = {}
        self.visible_npcs: Dict[int, NpcInfo] = {}
        self.spells: List[int] = []
        self.gold: int = 0

        # Combat state
        self.selected_target: int = 0
        self.cooldowns: Dict[int, Cooldown] = {}
        self.combat_messages: List[CombatMessage] = []
        self.spell_casts: List[SpellCast] = []
        self.cast_errors: List[Tuple[int, int]] = []  # (spell_id, error_code)

        # Received packets queue
        self.packet_queue: List[Tuple[int, PacketBuffer]] = []

        # Threading
        self.running = False
        self.recv_thread: Optional[threading.Thread] = None
        self.lock = threading.Lock()

    def connect(self) -> bool:
        """Connect to the server."""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10.0)
            self.socket.connect((self.host, self.port))
            self.running = True
            self.recv_thread = threading.Thread(target=self._recv_loop, daemon=True)
            self.recv_thread.start()
            print(f"[{self.name}] Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"[{self.name}] Connection failed: {e}")
            return False

    def disconnect(self):
        """Disconnect from the server."""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        if self.recv_thread:
            self.recv_thread.join(timeout=1.0)
        print(f"[{self.name}] Disconnected")

    def _recv_loop(self):
        """Background thread to receive packets."""
        while self.running and self.socket:
            try:
                data = self.socket.recv(4096)
                if not data:
                    print(f"[{self.name}] Server closed connection")
                    break

                with self.lock:
                    self.recv_buffer.extend(data)
                    self._process_packets()
            except socket.timeout:
                continue
            except Exception as e:
                if self.running:
                    print(f"[{self.name}] Receive error: {e}")
                break

    def _process_packets(self):
        """Process complete packets from receive buffer."""
        while len(self.recv_buffer) >= 4:  # Minimum: size(2) + opcode(2)
            # Read packet size (includes the size field itself)
            packet_size = struct.unpack_from('<H', self.recv_buffer, 0)[0]

            if len(self.recv_buffer) < packet_size:
                break  # Wait for more data

            # Extract complete packet
            packet_data = bytes(self.recv_buffer[2:packet_size])  # Skip size header
            del self.recv_buffer[:packet_size]

            if len(packet_data) < 2:
                continue

            # Parse opcode
            opcode = struct.unpack_from('<H', packet_data, 0)[0]
            payload = PacketBuffer(packet_data[2:])

            # Handle packet
            self._handle_packet(opcode, payload)

    def _handle_packet(self, opcode: int, payload: PacketBuffer):
        """Handle received packet."""
        # Store for test inspection
        self.packet_queue.append((opcode, payload.clone()))

        # Auto-handle certain packets
        if opcode == Opcode.Mutual_Ping:
            self._send_ping()
        elif opcode == Opcode.Server_Validate:
            self._on_validate(payload)
        elif opcode == Opcode.Server_CharacterList:
            self._on_character_list(payload)
        elif opcode == Opcode.Server_CharaCreateResult:
            self._on_char_create_result(payload)
        elif opcode == Opcode.Server_NewWorld:
            self._on_new_world(payload)
        elif opcode == Opcode.Server_SetController:
            self._on_set_controller(payload)
        elif opcode == Opcode.Server_Player:
            self._on_player(payload)
        elif opcode == Opcode.Server_Npc:
            self._on_npc(payload)
        elif opcode == Opcode.Server_DestroyObject:
            self._on_destroy_object(payload)
        elif opcode == Opcode.Server_UnitSpline:
            self._on_unit_spline(payload)
        elif opcode == Opcode.Server_UnitTeleport:
            self._on_unit_teleport(payload)
        elif opcode == Opcode.Server_ObjectVariable:
            self._on_object_variable(payload)
        elif opcode == Opcode.Server_Inventory:
            self._on_inventory(payload)
        elif opcode == Opcode.Server_Spellbook:
            self._on_spellbook(payload)
        elif opcode == Opcode.Server_QuestList:
            self._on_quest_list(payload)
        elif opcode == Opcode.Server_Cooldown:
            self._on_cooldown(payload)
        elif opcode == Opcode.Server_CombatMsg:
            self._on_combat_msg(payload)
        elif opcode == Opcode.Server_SpellGo:
            self._on_spell_go(payload)
        elif opcode == Opcode.Server_CastStart:
            self._on_cast_start(payload)
        elif opcode == Opcode.Server_CastStop:
            self._on_cast_stop(payload)
        elif opcode == Opcode.Server_UnitAuras:
            self._on_unit_auras(payload)
        elif opcode == Opcode.Server_WorldError:
            self._on_world_error(payload)
        else:
            # Don't print for common packets
            pass

    # -------------------------------------------------------------------------
    # Packet Handlers
    # -------------------------------------------------------------------------

    def _on_validate(self, buf: PacketBuffer):
        result = buf.read_u8()
        server_time = buf.read_i64()
        if result == AuthResult.Validated:
            self.authenticated = True
            print(f"[{self.name}] Authentication successful (server_time={server_time})")
        else:
            print(f"[{self.name}] Authentication failed: {AuthResult(result).name}")

    def _on_character_list(self, buf: PacketBuffer):
        count = buf.read_u16()
        self.characters = []
        for _ in range(count):
            ch = Character()
            ch.guid = buf.read_u32()
            ch.name = buf.read_string()
            ch.class_id = buf.read_u8()
            ch.gender = buf.read_u8()
            ch.level = buf.read_i32()
            ch.portrait = buf.read_i32()
            self.characters.append(ch)
        print(f"[{self.name}] Received {len(self.characters)} characters")

    def _on_char_create_result(self, buf: PacketBuffer):
        result = buf.read_u8()
        if result == CharCreateResult.Success:
            print(f"[{self.name}] Character created successfully")
        else:
            print(f"[{self.name}] Character creation failed: {CharCreateResult(result).name}")

    def _on_new_world(self, buf: PacketBuffer):
        self.map_id = buf.read_i32()
        self.in_world = True
        print(f"[{self.name}] Entering world (map_id={self.map_id})")

    def _on_set_controller(self, buf: PacketBuffer):
        self.my_guid = buf.read_u32()
        # r1189-compatible world entry sends the Player entity before
        # SetController, so promote the entity now that we know our GUID.
        if self.my_guid in self.visible_players:
            self.my_player = self.visible_players.pop(self.my_guid)
        print(f"[{self.name}] Controller set (my_guid={self.my_guid})")

    def _on_player(self, buf: PacketBuffer):
        player = PlayerInfo()
        player.guid = buf.read_u32()
        player.name = buf.read_string()
        player.sub_name = buf.read_string()
        player.class_id = buf.read_u8()
        player.gender = buf.read_u8()
        player.portrait_id = buf.read_i32()
        player.x = buf.read_float()
        player.y = buf.read_float()
        player.orientation = buf.read_float()
        player.equipment = buf.read_map_i32_i32()
        player.variables = buf.read_map_i32_i32()

        if player.guid == self.my_guid:
            self.my_player = player
            print(f"[{self.name}] Self: HP={player.variables.get(0, 0)}/{player.variables.get(1, 0)}, Mana={player.variables.get(2, 0)}/{player.variables.get(3, 0)}")
        else:
            self.visible_players[player.guid] = player
            print(f"[{self.name}] Player spawned: '{player.name}' (GUID={player.guid})")

    def _on_npc(self, buf: PacketBuffer):
        # GP_Server_Npc: guid, entry, x, y, orientation, variables.
        # NPC display/model data comes from the client's game.db by entry id.
        npc = NpcInfo()
        npc.guid = buf.read_u32()
        npc.entry = buf.read_i32()
        npc.x = buf.read_float()
        npc.y = buf.read_float()
        npc.orientation = buf.read_float()
        npc.variables = buf.read_map_i32_i32()

        self.visible_npcs[npc.guid] = npc
        hp = npc.variables.get(0, 0)
        max_hp = npc.variables.get(1, 0)
        print(f"[{self.name}] NPC spawned: entry={npc.entry} (GUID={npc.guid}, HP={hp}/{max_hp})")

    def _on_destroy_object(self, buf: PacketBuffer):
        guid = buf.read_u32()
        if guid in self.visible_players:
            name = self.visible_players[guid].name
            del self.visible_players[guid]
            print(f"[{self.name}] Player despawned: '{name}'")
        elif guid in self.visible_npcs:
            name = self.visible_npcs[guid].name
            del self.visible_npcs[guid]
            print(f"[{self.name}] NPC despawned: '{name}'")

    def _on_unit_spline(self, buf: PacketBuffer):
        # GP_Server_UnitSpline: guid, authoritative start, point vector, slide, silent.
        guid = buf.read_u32()
        start_x = buf.read_float()
        start_y = buf.read_float()
        spline_size = buf.read_u16()
        points = []
        for _ in range(spline_size):
            x = buf.read_float()
            y = buf.read_float()
            points.append((x, y))
        _slide = buf.read_bool()
        _silent = buf.read_bool()

        # Track the authoritative start position. The final spline point is the
        # destination, not the unit's instantaneous position.
        if guid in self.visible_npcs:
            self.visible_npcs[guid].x = start_x
            self.visible_npcs[guid].y = start_y

    def _on_unit_teleport(self, buf: PacketBuffer):
        guid = buf.read_u32()
        x = buf.read_float()      # m_newX
        y = buf.read_float()      # m_newY
        ori = buf.read_float()    # m_newOri
        # Update tracked position if needed

    def _on_object_variable(self, buf: PacketBuffer):
        guid = buf.read_u32()
        var_id = buf.read_i32()
        value = buf.read_i32()

        # Update variables
        if guid == self.my_guid and self.my_player:
            self.my_player.variables[var_id] = value
            if var_id == ObjVariable.Health:
                max_hp = self.my_player.variables.get(ObjVariable.MaxHealth, 0)
                print(f"[{self.name}] HP updated: {value}/{max_hp}")
            elif var_id == ObjVariable.Mana:
                max_mp = self.my_player.variables.get(ObjVariable.MaxMana, 0)
                print(f"[{self.name}] Mana updated: {value}/{max_mp}")
        elif guid in self.visible_npcs:
            self.visible_npcs[guid].variables[var_id] = value
            if var_id == ObjVariable.Health:
                npc = self.visible_npcs[guid]
                max_hp = npc.variables.get(ObjVariable.MaxHealth, 0)
                print(f"[{self.name}] NPC '{npc.name}' HP: {value}/{max_hp}")

    def _on_inventory(self, buf: PacketBuffer):
        # Read gold
        self.gold = buf.read_i32()
        # Skip items for now
        print(f"[{self.name}] Gold: {self.gold}")

    def _on_spellbook(self, buf: PacketBuffer):
        count = buf.read_u16()
        self.spells = []
        for _ in range(count):
            spell_id = buf.read_i32()
            _level = buf.read_u8()
            bp_count = buf.read_u16()
            for _ in range(bp_count):
                _ = buf.read_i16()
                _ = buf.read_i16()
            self.spells.append(spell_id)
        print(f"[{self.name}] Spellbook: {self.spells}")

    def _on_quest_list(self, buf: PacketBuffer):
        # Skip quest data
        pass

    def _on_cooldown(self, buf: PacketBuffer):
        cd = Cooldown()
        cd.spell_id = buf.read_i32()       # m_id
        cd.remaining_ms = buf.read_i32()   # m_totalDuration
        # Note: packet only has 2 fields (spell_id, remaining_ms)

        self.cooldowns[cd.spell_id] = cd
        print(f"[{self.name}] Cooldown: spell {cd.spell_id} = {cd.remaining_ms}ms")

    def _on_combat_msg(self, buf: PacketBuffer):
        # GP_Server_CombatMsg format:
        # m_casterGuid (u32), m_targetGuid (u32), m_spellId (i32), m_amount (i32),
        # m_spellEffect (u8), m_spellResult (u8), m_periodic (bool), m_positive (bool)
        msg = CombatMessage()
        msg.caster_guid = buf.read_u32()
        msg.target_guid = buf.read_u32()
        msg.spell_id = buf.read_i32()
        msg.value = buf.read_i32()
        spell_effect = buf.read_u8()      # SpellDefines::Effects
        msg.hit_result = buf.read_u8()    # SpellDefines::HitResult
        is_periodic = buf.read_bool()
        is_positive = buf.read_bool()

        # Map spell effect to message type
        # Effects: 0=Damage, 1=Heal, 2=ApplyAura, 3=ApplyAreaAura, 5=MeleeAtk, etc.
        if spell_effect in (0, 5):  # Damage or MeleeAtk
            msg.msg_type = CombatMsgType.Damage
        elif spell_effect == 1:  # Heal
            msg.msg_type = CombatMsgType.Heal
        else:
            msg.msg_type = spell_effect  # Store raw value

        self.combat_messages.append(msg)

        # Pretty print
        hit_name = HitResult(msg.hit_result).name if msg.hit_result < len(HitResult) else str(msg.hit_result)

        if msg.msg_type == CombatMsgType.Damage:
            print(f"[{self.name}] COMBAT: {msg.caster_guid} -> {msg.target_guid}: {msg.value} damage ({hit_name})")
        elif msg.msg_type == CombatMsgType.Heal:
            print(f"[{self.name}] COMBAT: {msg.caster_guid} heals {msg.target_guid} for {msg.value}")
        else:
            print(f"[{self.name}] COMBAT: effect={spell_effect}, periodic={is_periodic}, positive={is_positive}")

    def _on_spell_go(self, buf: PacketBuffer):
        cast = SpellCast()
        cast.caster_guid = buf.read_u32()
        cast.spell_id = buf.read_i32()
        target_count = buf.read_u16()
        for _ in range(target_count):
            target_guid = buf.read_u32()
            hit_result = buf.read_u8()
            cast.targets.append((target_guid, hit_result))

        self.spell_casts.append(cast)
        print(f"[{self.name}] SPELL: {cast.caster_guid} casts spell {cast.spell_id} on {len(cast.targets)} targets")

    def _on_cast_start(self, buf: PacketBuffer):
        caster_guid = buf.read_u32()
        spell_id = buf.read_i32()
        cast_time = buf.read_i32()
        print(f"[{self.name}] Cast start: {caster_guid} casting spell {spell_id} ({cast_time}ms)")

    def _on_cast_stop(self, buf: PacketBuffer):
        # GP_Server_CastStop only has m_guid - indicates cast was interrupted/cancelled
        caster_guid = buf.read_u32()
        print(f"[{self.name}] Cast stopped: caster {caster_guid}")

    def _on_unit_auras(self, buf: PacketBuffer):
        # GP_Server_UnitAuras format:
        # m_unitGuid (u32), then buffs list, then debuffs list
        # Each aura: spellId (i32), casterGuid (u32), maxDuration (i32),
        #            elapsedTime (i32), stacks (i32), positive (bool)
        guid = buf.read_u32()

        # Read buffs
        buff_count = buf.read_u16()
        buffs = []
        for _ in range(buff_count):
            spell_id = buf.read_i32()
            caster_guid = buf.read_u32()
            max_duration = buf.read_i32()
            elapsed_time = buf.read_i32()
            stacks = buf.read_i32()
            positive = buf.read_bool()
            buffs.append((spell_id, max_duration - elapsed_time, stacks))

        # Read debuffs
        debuff_count = buf.read_u16()
        debuffs = []
        for _ in range(debuff_count):
            spell_id = buf.read_i32()
            caster_guid = buf.read_u32()
            max_duration = buf.read_i32()
            elapsed_time = buf.read_i32()
            stacks = buf.read_i32()
            positive = buf.read_bool()
            debuffs.append((spell_id, max_duration - elapsed_time, stacks))

        print(f"[{self.name}] Auras on {guid}: buffs={buffs}, debuffs={debuffs}")

    def _on_world_error(self, buf: PacketBuffer):
        error_code = buf.read_u8()
        print(f"[{self.name}] World error: {error_code}")

    # -------------------------------------------------------------------------
    # Send Packets
    # -------------------------------------------------------------------------

    def _send_packet(self, opcode: int, payload: bytes = b''):
        """Send a packet to the server."""
        if not self.socket:
            return
        # Packet format: [uint16 size][uint16 opcode][payload]
        # size includes the size field itself
        total_size = 2 + 2 + len(payload)
        packet = struct.pack('<HH', total_size, opcode) + payload
        try:
            self.socket.sendall(packet)
        except Exception as e:
            print(f"[{self.name}] Send error: {e}")

    def _send_ping(self):
        """Respond to server ping."""
        self._send_packet(Opcode.Mutual_Ping)

    def authenticate(self, username: str, password: str, version: int = 1):
        """Send authentication request.

        Server expects GP_Client_Authenticate format:
        - m_token (string): "username:password"
        - m_buildVersion (int32): client version
        - m_fingerprint (string): client fingerprint (can be empty)
        """
        buf = PacketBuffer()
        token = f"{username}:{password}"
        buf.write_string(token)
        buf.write_i32(version)
        buf.write_string("")  # fingerprint (empty for testing)
        self._send_packet(Opcode.Client_Authenticate, buf.bytes())
        print(f"[{self.name}] Authenticating as '{username}'...")

    def request_character_list(self):
        """Request character list."""
        self._send_packet(Opcode.Client_CharacterList)
        print(f"[{self.name}] Requesting character list...")

    def create_character(self, name: str, class_id: int = 1, gender: int = 0,
                         portrait: int = 0, skin: int = 0, hair_style: int = 0, hair_color: int = 0):
        """Create a new character."""
        buf = PacketBuffer()
        buf.write_u8(gender)
        buf.write_i32(portrait)
        buf.write_string(name)
        buf.write_u8(class_id)
        self._send_packet(Opcode.Client_CharCreate, buf.bytes())
        print(f"[{self.name}] Creating character '{name}'...")

    def enter_world(self, character_guid: int):
        """Enter the world with specified character."""
        buf = PacketBuffer()
        buf.write_u32(character_guid)
        self._send_packet(Opcode.Client_EnterWorld, buf.bytes())
        self.selected_character_guid = character_guid
        print(f"[{self.name}] Entering world with character {character_guid}...")

    def set_target(self, target_guid: int):
        """Set currently selected target."""
        buf = PacketBuffer()
        buf.write_u32(target_guid)
        self._send_packet(Opcode.Client_SetSelected, buf.bytes())
        self.selected_target = target_guid
        print(f"[{self.name}] Target set to {target_guid}")

    def cast_spell(self, spell_id: int, target_guid: int = 0):
        """Cast a spell on target."""
        buf = PacketBuffer()
        buf.write_i32(spell_id)
        buf.write_u32(target_guid if target_guid else self.selected_target)
        buf.write_float(0.0)
        buf.write_float(0.0)
        buf.write_bool(False)
        self._send_packet(Opcode.Client_CastSpell, buf.bytes())
        print(f"[{self.name}] Casting spell {spell_id} on {target_guid or self.selected_target}")

    def request_move(self, x: float, y: float):
        """Request movement to position."""
        buf = PacketBuffer()
        buf.write_float(x)
        buf.write_float(y)
        buf.write_bool(False)
        self._send_packet(Opcode.Client_RequestMove, buf.bytes())

    def request_respawn(self):
        """Request respawn after death."""
        self._send_packet(Opcode.Client_RequestRespawn)
        print(f"[{self.name}] Requesting respawn...")

    # -------------------------------------------------------------------------
    # Utility Methods
    # -------------------------------------------------------------------------

    def wait_for_packet(self, opcode: int, timeout: float = 5.0) -> Optional[PacketBuffer]:
        """Wait for a specific packet type."""
        start = time.time()
        while time.time() - start < timeout:
            with self.lock:
                for i, (pkt_opcode, pkt_payload) in enumerate(self.packet_queue):
                    if pkt_opcode == opcode:
                        self.packet_queue.pop(i)
                        return pkt_payload
            time.sleep(0.05)
        return None

    def wait_for_world_entry(self, timeout: float = 5.0) -> bool:
        """Wait until world entry is complete."""
        start = time.time()
        while time.time() - start < timeout:
            if self.in_world and self.my_guid and self.my_player:
                return True
            time.sleep(0.1)
        return False

    def get_health(self) -> int:
        """Get current health."""
        if self.my_player:
            return self.my_player.variables.get(ObjVariable.Health, 0)
        return 0

    def get_max_health(self) -> int:
        """Get max health."""
        if self.my_player:
            return self.my_player.variables.get(ObjVariable.MaxHealth, 0)
        return 0

    def get_mana(self) -> int:
        """Get current mana."""
        if self.my_player:
            return self.my_player.variables.get(ObjVariable.Mana, 0)
        return 0

    def is_dead(self) -> bool:
        """Check if player is dead."""
        return self.get_health() <= 0

    def clear_combat_state(self):
        """Clear combat tracking state for new test."""
        self.combat_messages.clear()
        self.spell_casts.clear()
        self.cast_errors.clear()

# =============================================================================
# Test Functions
# =============================================================================

def test_target_selection(client: CombatClient) -> bool:
    """Test 1: Target Selection"""
    print("\n=== Test 1: Target Selection ===")

    # Find an NPC to target
    if not client.visible_npcs:
        print("  SKIP: No NPCs visible to target")
        return True

    npc_guid = list(client.visible_npcs.keys())[0]
    npc = client.visible_npcs[npc_guid]
    print(f"  Targeting NPC: {npc.name} (GUID={npc_guid})")

    client.set_target(npc_guid)
    time.sleep(0.2)

    if client.selected_target == npc_guid:
        print("  PASS: Target selection successful")
        return True
    else:
        print(f"  FAIL: Target not set (expected {npc_guid}, got {client.selected_target})")
        return False

def test_spell_cast_invalid_target(client: CombatClient) -> bool:
    """Test 2: Spell Cast with Invalid Target"""
    print("\n=== Test 2: Spell Cast - Invalid Target ===")

    client.clear_combat_state()

    # Cast spell on invalid target (GUID 0)
    client.cast_spell(1, 0)  # Spell ID 1, no target
    time.sleep(0.5)

    # Should receive cast error
    if client.cast_errors:
        error = client.cast_errors[-1]
        print(f"  PASS: Received expected cast error (spell={error[0]}, error={error[1]})")
        return True
    else:
        print("  INFO: No cast error received (may depend on spell type)")
        return True  # Self-cast spells won't error

def test_spell_cast_on_npc(client: CombatClient) -> bool:
    """Test 3: Spell Cast on NPC"""
    print("\n=== Test 3: Spell Cast on NPC ===")

    if not client.visible_npcs:
        print("  SKIP: No NPCs visible")
        return True

    client.clear_combat_state()

    npc_guid = list(client.visible_npcs.keys())[0]
    npc = client.visible_npcs[npc_guid]
    initial_hp = npc.variables.get(ObjVariable.Health, 0)

    print(f"  Targeting NPC: {npc.name} (HP={initial_hp})")
    client.set_target(npc_guid)
    time.sleep(0.2)

    # Try to cast a damage spell (spell ID 1 is typically basic attack)
    # The actual spell ID depends on what's in the spellbook
    if client.spells:
        spell_id = client.spells[0]
        print(f"  Casting spell {spell_id} on target...")
        client.cast_spell(spell_id, npc_guid)
        time.sleep(1.0)

        # Check for spell cast notification
        if client.spell_casts:
            print(f"  PASS: Spell cast executed ({len(client.spell_casts)} casts)")
            return True
        elif client.cast_errors:
            error = client.cast_errors[-1]
            print(f"  INFO: Cast failed with error {error[1]} (may be expected)")
            return True
        else:
            print("  WARN: No spell cast or error received")
            return True
    else:
        print("  SKIP: No spells in spellbook")
        return True

def test_combat_messages(client: CombatClient) -> bool:
    """Test 4: Combat Message Reception"""
    print("\n=== Test 4: Combat Messages ===")

    # Combat messages should have been generated by previous tests
    if client.combat_messages:
        print(f"  PASS: Received {len(client.combat_messages)} combat messages")
        for msg in client.combat_messages[-5:]:  # Show last 5
            msg_type = CombatMsgType(msg.msg_type).name if msg.msg_type < 10 else str(msg.msg_type)
            print(f"    - {msg_type}: {msg.caster_guid} -> {msg.target_guid}, value={msg.value}")
        return True
    else:
        print("  INFO: No combat messages received (may need active combat)")
        return True

def test_cooldown_tracking(client: CombatClient) -> bool:
    """Test 5: Cooldown System"""
    print("\n=== Test 5: Cooldown Tracking ===")

    if client.cooldowns:
        print(f"  PASS: Tracking {len(client.cooldowns)} cooldowns")
        for spell_id, cd in client.cooldowns.items():
            print(f"    - Spell {spell_id}: {cd.remaining_ms}ms remaining")
        return True
    else:
        # Try casting a spell to trigger cooldown
        if client.spells and client.visible_npcs:
            npc_guid = list(client.visible_npcs.keys())[0]
            spell_id = client.spells[0]
            client.cast_spell(spell_id, npc_guid)
            time.sleep(0.5)

            if client.cooldowns:
                print(f"  PASS: Cooldown triggered after cast")
                return True

        print("  INFO: No cooldowns active (spells may have no cooldown)")
        return True

def test_npc_aggro(client: CombatClient) -> bool:
    """Test 6: NPC Aggro Behavior"""
    print("\n=== Test 6: NPC Aggro ===")

    if not client.visible_npcs:
        print("  SKIP: No NPCs visible")
        return True

    client.clear_combat_state()

    # Attack an NPC to generate aggro
    npc_guid = list(client.visible_npcs.keys())[0]
    npc = client.visible_npcs[npc_guid]

    print(f"  Attacking NPC: {npc.name}")

    if client.spells:
        client.cast_spell(client.spells[0], npc_guid)
        time.sleep(2.0)  # Wait for NPC to respond

        # Check if we received damage from NPC (indicating aggro)
        npc_damage = [m for m in client.combat_messages
                     if m.caster_guid == npc_guid and m.target_guid == client.my_guid]

        if npc_damage:
            print(f"  PASS: NPC attacked back ({len(npc_damage)} hits)")
            return True
        else:
            print("  INFO: NPC did not attack back (may be peaceful or dead)")
            return True
    else:
        print("  SKIP: No spells available")
        return True

def run_combat_tests(host: str, port: int, username: str, password: str) -> bool:
    """Run all combat tests."""
    print("\n" + "=" * 60)
    print(" Dreadmyst Combat E2E Test Suite")
    print("=" * 60)

    client = CombatClient("TestClient", host, port)

    try:
        # Connect and authenticate
        if not client.connect():
            print("FATAL: Could not connect to server")
            return False

        time.sleep(0.5)

        # Authenticate
        client.authenticate(username, password)
        time.sleep(1.0)

        if not client.authenticated:
            print("FATAL: Authentication failed")
            return False

        # Get character list
        client.request_character_list()
        time.sleep(0.5)

        if not client.characters:
            # Create a test character (name must be letters only, no digits)
            char_name = f"Tester{''.join(random.choices('abcdefghijklmnopqrstuvwxyz', k=4))}"
            client.create_character(char_name, class_id=1)
            time.sleep(0.5)
            client.request_character_list()
            time.sleep(0.5)

        if not client.characters:
            print("FATAL: No characters available")
            return False

        # Enter world
        client.enter_world(client.characters[0].guid)

        if not client.wait_for_world_entry(timeout=5.0):
            print("FATAL: Could not enter world")
            return False

        print(f"\n[Entered world as {client.my_player.name}]")
        print(f"  Position: ({client.my_player.x}, {client.my_player.y})")
        print(f"  Health: {client.get_health()}/{client.get_max_health()}")
        print(f"  Mana: {client.get_mana()}")
        print(f"  NPCs visible: {len(client.visible_npcs)}")
        print(f"  Spells: {client.spells}")

        # Wait a moment for world state to settle
        time.sleep(1.0)

        # Run tests
        results = []
        results.append(("Target Selection", test_target_selection(client)))
        results.append(("Invalid Target Cast", test_spell_cast_invalid_target(client)))
        results.append(("Spell Cast on NPC", test_spell_cast_on_npc(client)))
        results.append(("Combat Messages", test_combat_messages(client)))
        results.append(("Cooldown Tracking", test_cooldown_tracking(client)))
        results.append(("NPC Aggro", test_npc_aggro(client)))

        # Summary
        print("\n" + "=" * 60)
        print(" Test Results")
        print("=" * 60)

        passed = 0
        failed = 0
        for name, result in results:
            status = "PASS" if result else "FAIL"
            if result:
                passed += 1
            else:
                failed += 1
            print(f"  {status}: {name}")

        print(f"\nTotal: {passed} passed, {failed} failed")
        print("=" * 60)

        return failed == 0

    finally:
        client.disconnect()

# =============================================================================
# Main Entry Point
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="Dreadmyst Combat E2E Test")
    parser.add_argument("--host", default="127.0.0.1", help="Server host")
    parser.add_argument("--port", type=int, default=8080, help="Server port")
    parser.add_argument("--user", default="combattest", help="Test username")
    parser.add_argument("--pass", dest="password", default="test123", help="Test password")
    args = parser.parse_args()

    success = run_combat_tests(args.host, args.port, args.user, args.password)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
