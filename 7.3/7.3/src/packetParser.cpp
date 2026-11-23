#include "packetParser.h"

#include "packetCodec.h"

void parserReset(PacketParser *parser)
{
    parser->state = PARSER_WAIT_START;
    parser->index = 0;
}

static uint8_t announcedPayloadLength(const PacketParser *parser)
{
    return parser->raw[PACKET_LEN_OFFSET];
}

static ParseResult failAndResync(PacketParser *parser, ParseResult reason)
{
    parserReset(parser);
    return reason;
}

// Octetii de dinaintea unui STX sunt gunoi de pe fir (zgomot la pornire sau
// restul unui pachet rupt) si se arunca fara sa fie o eroare.
static ParseResult consumeStart(PacketParser *parser, uint8_t byte)
{
    if (byte != PACKET_START) return PARSE_NEED_MORE;

    parser->raw[PACKET_START_OFFSET] = byte;
    parser->index = 1;
    parser->state = PARSER_HEADER;
    return PARSE_NEED_MORE;
}

static ParseResult consumeHeader(PacketParser *parser, uint8_t byte)
{
    parser->raw[parser->index++] = byte;
    if (parser->index < PACKET_PAYLOAD_OFFSET) return PARSE_NEED_MORE;

    uint8_t payloadLength = announcedPayloadLength(parser);
    if (payloadLength > PACKET_MAX_PAYLOAD) return failAndResync(parser, PARSE_BAD_LENGTH);

    parser->state = (payloadLength > 0) ? PARSER_PAYLOAD : PARSER_CHECKSUM;
    return PARSE_NEED_MORE;
}

static ParseResult consumePayload(PacketParser *parser, uint8_t byte)
{
    parser->raw[parser->index++] = byte;

    uint8_t payloadEnd = (uint8_t)(PACKET_PAYLOAD_OFFSET + announcedPayloadLength(parser));
    if (parser->index < payloadEnd) return PARSE_NEED_MORE;

    parser->state = PARSER_CHECKSUM;
    return PARSE_NEED_MORE;
}

static ParseResult consumeChecksum(PacketParser *parser, uint8_t byte)
{
    uint8_t expected = packetChecksum(parser->raw, announcedPayloadLength(parser));
    if (byte != expected) return failAndResync(parser, PARSE_BAD_CHECKSUM);

    parser->raw[parser->index++] = byte;
    parser->state = PARSER_END;
    return PARSE_NEED_MORE;
}

static void copyToPacket(const uint8_t *raw, BinaryPacket *packetOut)
{
    packetOut->deviceId = raw[PACKET_ID_OFFSET];
    packetOut->sequence = raw[PACKET_SEQ_OFFSET];
    packetOut->command = raw[PACKET_CMD_OFFSET];
    packetOut->payloadLength = raw[PACKET_LEN_OFFSET];

    for (uint8_t i = 0; i < packetOut->payloadLength; i++)
    {
        packetOut->payload[i] = raw[PACKET_PAYLOAD_OFFSET + i];
    }
}

// ETX este a doua verificare de structura: o suma de control nimerita din
// intamplare pe un flux decalat tot cade aici.
static ParseResult consumeEnd(PacketParser *parser, uint8_t byte, BinaryPacket *packetOut)
{
    if (byte != PACKET_END) return failAndResync(parser, PARSE_BAD_END);

    copyToPacket(parser->raw, packetOut);
    parserReset(parser);
    return PARSE_DONE;
}

ParseResult parserFeed(PacketParser *parser, uint8_t byte, BinaryPacket *packetOut)
{
    switch (parser->state)
    {
        case PARSER_WAIT_START: return consumeStart(parser, byte);
        case PARSER_HEADER:     return consumeHeader(parser, byte);
        case PARSER_PAYLOAD:    return consumePayload(parser, byte);
        case PARSER_CHECKSUM:   return consumeChecksum(parser, byte);
        case PARSER_END:        return consumeEnd(parser, byte, packetOut);
    }
    return failAndResync(parser, PARSE_NEED_MORE);
}

const char *parseResultName(ParseResult result)
{
    switch (result)
    {
        case PARSE_NEED_MORE:    return "incomplet";
        case PARSE_DONE:         return "ok";
        case PARSE_BAD_LENGTH:   return "LEN gresit";
        case PARSE_BAD_CHECKSUM: return "suma gresita";
        case PARSE_BAD_END:      return "ETX lipsa";
    }
    return "necunoscut";
}
