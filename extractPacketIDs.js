import { readFileSync, writeFileSync } from 'fs';

const packetJSONFile = './.cache/generated/reports/packets.json';
const headerFile = './src/packetTypes.h'

const json = JSON.parse(readFileSync(packetJSONFile, 'utf8'))


let out = '#pragma once\n\n' + Object.entries(json).map(([g, cs]) => Object.entries(cs).map(([l, types]) => {
  let t = l === 'serverbound' ? 'SB' : l === 'clientbound' ? 'CB' : 'other';
  let values = Object.entries(types).map(([key, value]) => `PTYPE_${g.toUpperCase()}_${t}_${key.replace('minecraft:', '').toUpperCase()} = ${value.protocol_id}`);

  let result = `typedef enum MCAPI_${g.toUpperCase()}_${l.toUpperCase()} {\n  ${values.join(',\n  ')}\n} PTYPE_${g.toUpperCase()}_${l.toUpperCase()};\n`;
  return result;
})).flat().join('\n\n');

writeFileSync(headerFile, out);
