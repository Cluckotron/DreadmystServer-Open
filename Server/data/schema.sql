-- Dreadmyst Server Database Schema
-- Task 2.2: Server Database Schema
-- Auto-creates tables on first server run

-- ============================================================================
-- Account Management
-- ============================================================================

CREATE TABLE IF NOT EXISTS accounts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL COLLATE NOCASE,
    password_hash TEXT NOT NULL,
    email TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_login DATETIME,
    banned_until DATETIME,
    ban_reason TEXT,
    is_gm INTEGER DEFAULT 0,
    failed_logins INTEGER DEFAULT 0,
    last_failed_login DATETIME
);

CREATE INDEX IF NOT EXISTS idx_accounts_username ON accounts(username);

-- ============================================================================
-- Characters
-- ============================================================================

CREATE TABLE IF NOT EXISTS characters (
    guid INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id INTEGER NOT NULL,
    name TEXT UNIQUE NOT NULL COLLATE NOCASE,
    class_id INTEGER NOT NULL,
    gender INTEGER NOT NULL,
    level INTEGER DEFAULT 1,
    experience INTEGER DEFAULT 0,
    portrait_id INTEGER DEFAULT 0,
    skin_color INTEGER DEFAULT 0,
    hair_style INTEGER DEFAULT 0,
    hair_color INTEGER DEFAULT 0,
    map_id INTEGER DEFAULT 1,
    position_x REAL DEFAULT 0,
    position_y REAL DEFAULT 0,
    facing REAL DEFAULT 0,
    health INTEGER DEFAULT 100,
    max_health INTEGER DEFAULT 100,
    mana INTEGER DEFAULT 50,
    max_mana INTEGER DEFAULT 50,
    gold INTEGER DEFAULT 0,
    played_time INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_login DATETIME,
    last_logout DATETIME,
    deleted_at DATETIME,
    is_deleted INTEGER DEFAULT 0,
    FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_characters_account ON characters(account_id);
CREATE INDEX IF NOT EXISTS idx_characters_name ON characters(name);

-- ============================================================================
-- Character Inventory
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_inventory (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    character_guid INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    stack_count INTEGER DEFAULT 1,
    durability INTEGER DEFAULT 100,
    enchant_id INTEGER DEFAULT 0,
    flags INTEGER DEFAULT 0,
    UNIQUE(character_guid, slot),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_inventory_character ON character_inventory(character_guid);

-- ============================================================================
-- Character Equipment (separate from inventory for easier querying)
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_equipment (
    character_guid INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    durability INTEGER DEFAULT 100,
    enchant_id INTEGER DEFAULT 0,
    affix1 INTEGER DEFAULT 0,
    affix2 INTEGER DEFAULT 0,
    gem1 INTEGER DEFAULT 0,
    gem2 INTEGER DEFAULT 0,
    gem3 INTEGER DEFAULT 0,
    PRIMARY KEY (character_guid, slot),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

-- ============================================================================
-- Character Spells
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_spells (
    character_guid INTEGER NOT NULL,
    spell_id INTEGER NOT NULL,
    rank INTEGER DEFAULT 1,
    learned_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (character_guid, spell_id),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_spells_character ON character_spells(character_guid);

-- ============================================================================
-- Character Stat Bonuses
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_stat_bonuses (
    character_guid INTEGER NOT NULL,
    stat_id INTEGER NOT NULL,
    bonus INTEGER NOT NULL,
    PRIMARY KEY (character_guid, stat_id),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_stat_bonuses_character ON character_stat_bonuses(character_guid);

-- ============================================================================
-- Character Quests
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_quests (
    character_guid INTEGER NOT NULL,
    quest_id INTEGER NOT NULL,
    status INTEGER NOT NULL DEFAULT 0,
    progress TEXT,
    accepted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    completed_at DATETIME,
    PRIMARY KEY (character_guid, quest_id),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_quests_character ON character_quests(character_guid);

-- ============================================================================
-- Character Bank
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_bank (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    character_guid INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    stack_count INTEGER DEFAULT 1,
    durability INTEGER DEFAULT 100,
    enchant_id INTEGER DEFAULT 0,
    UNIQUE(character_guid, slot),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_bank_character ON character_bank(character_guid);

-- ============================================================================
-- Character Cooldowns (for spell/item cooldowns that persist across sessions)
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_cooldowns (
    character_guid INTEGER NOT NULL,
    spell_id INTEGER NOT NULL,
    cooldown_end DATETIME NOT NULL,
    PRIMARY KEY (character_guid, spell_id),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

-- ============================================================================
-- Character Action Bars (hotkey bindings)
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_actionbars (
    character_guid INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    action_type INTEGER NOT NULL,
    action_id INTEGER NOT NULL,
    PRIMARY KEY (character_guid, slot),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

-- ============================================================================
-- Guilds
-- ============================================================================

CREATE TABLE IF NOT EXISTS guilds (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT UNIQUE NOT NULL COLLATE NOCASE,
    leader_guid INTEGER NOT NULL,
    motd TEXT DEFAULT '',
    info TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    bank_gold INTEGER DEFAULT 0,
    FOREIGN KEY (leader_guid) REFERENCES characters(guid)
);

CREATE INDEX IF NOT EXISTS idx_guilds_name ON guilds(name);
CREATE INDEX IF NOT EXISTS idx_guilds_leader ON guilds(leader_guid);

-- ============================================================================
-- Guild Members
-- ============================================================================

CREATE TABLE IF NOT EXISTS guild_members (
    guild_id INTEGER NOT NULL,
    character_guid INTEGER NOT NULL,
    rank INTEGER DEFAULT 0,
    note TEXT DEFAULT '',
    officer_note TEXT DEFAULT '',
    joined_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (guild_id, character_guid),
    FOREIGN KEY (guild_id) REFERENCES guilds(id) ON DELETE CASCADE,
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_guild_members_character ON guild_members(character_guid);

-- ============================================================================
-- Guild Ranks (custom rank names and permissions)
-- ============================================================================

CREATE TABLE IF NOT EXISTS guild_ranks (
    guild_id INTEGER NOT NULL,
    rank_id INTEGER NOT NULL,
    name TEXT NOT NULL,
    permissions INTEGER DEFAULT 0,
    PRIMARY KEY (guild_id, rank_id),
    FOREIGN KEY (guild_id) REFERENCES guilds(id) ON DELETE CASCADE
);

-- ============================================================================
-- Guild Bank
-- ============================================================================

CREATE TABLE IF NOT EXISTS guild_bank (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    guild_id INTEGER NOT NULL,
    tab INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    stack_count INTEGER DEFAULT 1,
    UNIQUE(guild_id, tab, slot),
    FOREIGN KEY (guild_id) REFERENCES guilds(id) ON DELETE CASCADE
);

-- ============================================================================
-- Mail System
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_mail (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    sender_guid INTEGER NOT NULL,
    receiver_guid INTEGER NOT NULL,
    subject TEXT NOT NULL,
    body TEXT DEFAULT '',
    gold INTEGER DEFAULT 0,
    sent_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    expires_at DATETIME,
    is_read INTEGER DEFAULT 0,
    has_attachment INTEGER DEFAULT 0,
    FOREIGN KEY (sender_guid) REFERENCES characters(guid),
    FOREIGN KEY (receiver_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_mail_receiver ON character_mail(receiver_guid);

-- ============================================================================
-- Mail Attachments
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_mail_items (
    mail_id INTEGER NOT NULL,
    slot INTEGER NOT NULL,
    item_id INTEGER NOT NULL,
    stack_count INTEGER DEFAULT 1,
    PRIMARY KEY (mail_id, slot),
    FOREIGN KEY (mail_id) REFERENCES character_mail(id) ON DELETE CASCADE
);

-- ============================================================================
-- Friends and Ignore Lists
-- ============================================================================

CREATE TABLE IF NOT EXISTS character_social (
    character_guid INTEGER NOT NULL,
    friend_guid INTEGER NOT NULL,
    flags INTEGER DEFAULT 0,
    note TEXT DEFAULT '',
    PRIMARY KEY (character_guid, friend_guid),
    FOREIGN KEY (character_guid) REFERENCES characters(guid) ON DELETE CASCADE,
    FOREIGN KEY (friend_guid) REFERENCES characters(guid) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_social_character ON character_social(character_guid);

-- ============================================================================
-- Server Variables (key-value store for server state)
-- ============================================================================

CREATE TABLE IF NOT EXISTS server_variables (
    key TEXT PRIMARY KEY NOT NULL,
    value TEXT NOT NULL,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Insert schema version for migrations
INSERT OR REPLACE INTO server_variables (key, value) VALUES ('schema_version', '1');

-- Private-server migration: old soft-deleted rows were left under a UNIQUE name,
-- which made deleted character names impossible to reuse. Tombstone them once.
UPDATE characters
SET name = '__deleted_' || guid || '_' || name
WHERE is_deleted = 1 AND name NOT LIKE '__deleted_%';
