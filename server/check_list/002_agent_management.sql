-- Self-hosted agent management schema additions.
-- Replaces the xiaozhi.me-backed agent config / device binding with local MySQL.
-- Apply after check_list/create_mysql_database.sql (the original schema).
--
-- Charset/collation is declared explicitly (utf8mb4 / utf8mb4_0900_ai_ci) to match
-- the rest of this schema (see deployment notes: MySQL 8.0 was chosen specifically
-- because this collation isn't available in MariaDB) rather than relying on
-- whatever the connecting session's default happens to be -- this matters because
-- `chat_message.device_mac` has a foreign key to `device.mac`, and MySQL requires
-- matching charset/collation between a FK column and the column it references.

create table stackChan.agent
(
    id                bigint auto_increment
        primary key,
    name              varchar(255)                          not null comment 'Agent config bundle name',
    assistant_name    varchar(255)                          null comment 'Spoken assistant name',
    persona           text                                  null comment 'Character / system-prompt text',
    llm_model         varchar(100)                          null comment 'Inert until Path #2 (self-hosted voice backend) exists to read it -- see note below',
    tts_voice         varchar(100)                          null comment 'Inert until Path #2 exists to read it',
    tts_speech_speed  varchar(20)                           null,
    tts_pitch         int                                   null,
    asr_speed         varchar(20)                           null,
    language          varchar(10) default 'en'              not null,
    memory            text                                  null,
    memory_type       varchar(20) default 'OFF'             not null,
    is_default        tinyint(1)  default 0                 not null comment 'Which row RestoreDefaultAgent resets a device to; app enforces at most one',
    created_at        datetime    default CURRENT_TIMESTAMP not null,
    updated_at        datetime    default CURRENT_TIMESTAMP not null on update CURRENT_TIMESTAMP
)
    default character set utf8mb4 collate utf8mb4_0900_ai_ci
    comment 'Locally-managed agent config bundles (persona/voice/LLM), replacing xiaozhi.me agents';

alter table stackChan.device
    add column agent_id bigint null comment 'Bound agent id' after bind_time,
    add constraint fk_device_agent
        foreign key (agent_id) references stackChan.agent (id)
            on update cascade on delete set null;

create index idx_device_agent_id
    on stackChan.device (agent_id);

create table stackChan.chat_message
(
    id          bigint auto_increment
        primary key,
    agent_id    bigint                             not null,
    device_mac  varchar(17)                        null,
    role        enum ('user', 'assistant')         not null,
    content     text                               not null,
    created_at  datetime default CURRENT_TIMESTAMP not null,
    constraint fk_chat_message_agent
        foreign key (agent_id) references stackChan.agent (id)
            on delete cascade,
    constraint fk_chat_message_device
        foreign key (device_mac) references stackChan.device (mac)
            on delete set null
)
    default character set utf8mb4 collate utf8mb4_0900_ai_ci
    comment 'Chat history, keyed by agent and (optionally) device. Not populated until the voice pipeline (Path #2) exists.';

create index idx_chat_message_agent_id
    on stackChan.chat_message (agent_id);

create index idx_chat_message_device_mac
    on stackChan.chat_message (device_mac);

-- Seed a single generic default persona for this exploratory-testing phase.
--
-- IMPORTANT, re: llm_model / tts_voice: these are left EMPTY on purpose. Nothing
-- reads this row to actually run an LLM or TTS voice today -- the live voice
-- conversation still runs entirely through xiaozhi.me/tenclass.net (Path #2, the
-- self-hosted OTA/websocket voice backend, has not been built yet). Filling these
-- in with a real model/voice name now would be cosmetic, not functional -- do that
-- once Path #2 exists and actually consumes this table. `persona`/`assistant_name`
-- are real today in the sense that they're stored and editable via the new
-- /admin/stackChan/agent API, just not yet connected to anything that talks.
insert into stackChan.agent
    (name, assistant_name, persona, llm_model, tts_voice, tts_speech_speed, tts_pitch, asr_speed, language, memory, memory_type, is_default)
values
    ('ChefBot Default', 'ChefBot',
     'You are ChefBot, a friendly and knowledgeable cooking assistant. Keep answers practical and concise.',
     '', '', '1.0', 0, '1.0', 'en', '', 'OFF', 1);
