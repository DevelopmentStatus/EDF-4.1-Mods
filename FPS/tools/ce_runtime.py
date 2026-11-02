"""Use the supplied Cheat Engine named-pipe bridge for this mod's runtime work."""
import sys
sys.dont_write_bytecode = True
import argparse, json
from pathlib import Path
sys.path.insert(0, r'A:\gameassets\ReverseEngineeringPackage\cheatengine-mcp-bridge-main\MCP_Server')
from mcp_cheatengine import ce_client

p=argparse.ArgumentParser()
p.add_argument('method')
p.add_argument('--lua')
p.add_argument('--params')
p.add_argument('--params-file')
p.add_argument('--attach')
p.add_argument('--out')
a=p.parse_args()
params=json.loads(a.params) if a.params else {}
if a.params_file: params=json.loads(Path(a.params_file).read_text(encoding='utf-8-sig'))
if a.attach: params={'process_id_or_name':a.attach}
if a.lua: params={'code':Path(a.lua).read_text(encoding='utf-8')}
try:
    result=ce_client.send_command(a.method,params)
    text=json.dumps(result,indent=2,ensure_ascii=False)
    if a.out: Path(a.out).write_text(text,encoding='utf-8')
    print(text)
finally:
    ce_client.close()
