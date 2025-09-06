import json, math, random, pickle
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Tuple, List

SAVE_DIR = Path("./saves"); SAVE_DIR.mkdir(parents=True, exist_ok=True)
STATE_PATH = SAVE_DIR / "brain_state.pkl"
FOOD_PRICE = 5
ACTIONS = list(range(17))

def clamp(v, lo, hi): return lo if v<lo else hi if v>hi else v
def dist2(ax, ay, bx, by): dx=ax-bx; dy=ay-by; return dx*dx+dy*dy
def rect_contains(rx, ry, rw, rh, x, y): return (rx<=x<=rx+rw) and (ry<=y<=ry+rh)
def unit_towards(ax, ay, bx, by):
  dx, dy = bx-ax, by-ay; d = (dx*dx+dy*dy) ** 0.5
  return (0.0,0.0) if d==0 else (dx/d, dy/d)

@dataclass
class Agent:
  name: str
  epsilon: float = 0.10
  alpha: float = 0.25
  gamma: float = 0.96
  q: Dict[Tuple[int, ...], List[float]] = field(default_factory=dict)
  last_state: Tuple[int, ...] = None
  last_action: int = None
  rng: random.Random = field(default_factory=random.Random)

  def _ensure(self,s): 
    if s not in self.q: self.q[s]=[0.0]*len(ACTIONS)
    return self.q[s]
  def select_action(self,s):
    self._ensure(s)
    if self.rng.random()<self.epsilon: return self.rng.choice(ACTIONS)
    qs=self.q[s]; m=max(qs); idx=[i for i,v in enumerate(qs) if v==m]; return self.rng.choice(idx)
  def update(self,s,a,r,sp):
    self._ensure(s); self._ensure(sp)
    self.q[s][a] += self.alpha * (r + self.gamma*max(self.q[sp]) - self.q[s][a])

class Brain:
  def __init__(self):
    self.agents: Dict[str,Agent]={}
    self.tick=0; self.dt=0.016
    self.bounds={"w":2048,"h":2048}
    self.store={"x":0,"y":0,"w":360,"h":360}
    self.recharge={"x":2048-360,"y":0,"w":360,"h":360}
    self.snapshot_every=360

  def api_init(self, cfg_json):
    cfg=json.loads(cfg_json) if cfg_json else {}
    self.bounds=cfg.get("bounds",self.bounds)
    self.store=cfg.get("store",self.store)
    self.recharge=cfg.get("recharge",self.recharge)
    for n in cfg.get("players",[]):
      if n not in self.agents:
        a=Agent(name=n); a.rng.seed(sum(ord(c) for c in n)); self.agents[n]=a
    self._save(); return {"ok":True,"players":list(self.agents.keys()),"food_price":FOOD_PRICE}

  def api_reward(self, player, value, reason):
    a=self.agents.get(player)
    if not a or a.last_state is None or a.last_action is None: return {"ok":False}
    a.update(a.last_state,a.last_action,float(value),a.last_state); return {"ok":True}

  def api_tick(self, world_json):
    w=json.loads(world_json); self.tick=int(w.get("tick",self.tick+1)); self.dt=float(w.get("dt",self.dt))
    self.bounds=w.get("bounds",self.bounds); self.store=w.get("store",self.store); self.recharge=w.get("recharge",self.recharge)
    coins=w.get("coins",[]); players=w.get("players",{})

    dec={}
    for name,p in players.items():
      a=self.agents.setdefault(name,Agent(name=name))
      obs=self._obs(name,p,players,coins); s=self._disc(obs); act=a.select_action(s)
      ux,uy,beh=self._policy(act,name,p,obs,coins)

      prox = obs["crowd"]["foes"] + obs["crowd"]["friends"]
      r = -0.01 * prox
      if obs["in_store"] and (p.get("coins",0)>=FOOD_PRICE or p.get("food",0)>0): r += 0.05
      if obs["in_recharge"] and p.get("energy",100.0)<90: r += 0.05
      if a.last_state is not None and a.last_action is not None: a.update(a.last_state,a.last_action,r,s)
      a.last_state=s; a.last_action=act
      dec[name]={"vx":ux,"vy":uy,"intent":beh,"hud":self._hud(name,p,beh)}
    if (self.tick%180)==0: self._save()
    return {"ok":True,"decisions":dec}

  def api_save(self): self._save(); return {"ok":True}
  def _save(self):
    blob={"tick":self.tick,"bounds":self.bounds,"store":self.store,"recharge":self.recharge,
          "agents":{k:{"epsilon":ag.epsilon,"alpha":ag.alpha,"gamma":ag.gamma,"q":ag.q} for k,ag in self.agents.items()}}
    with open(STATE_PATH,"wb") as f: pickle.dump(blob,f)

  def _nearest_coin(self,px,py,coins):
    if not coins: return None, None
    best=None; bd=1e30
    for c in coins:
      d=dist2(px,py,c["x"],c["y"])
      if d<bd: bd=d; best=c
    return best, bd

  def _obs(self,me,mep,players,coins):
    px,py=mep.get("x",0.0),mep.get("y",0.0)
    fr=fo=0
    for n,p in players.items():
      if n==me: continue
      d2=dist2(px,py,p.get("x",0.0),p.get("y",0.0))
      if d2<=200*200:
        sm=mep.get("coins",0)+mep.get("food",0)
        so=p.get("coins",0)+p.get("food",0)
        if so>sm: fo+=1
        else: fr+=1
    coin,_=self._nearest_coin(px,py,coins)
    in_store=rect_contains(self.store["x"],self.store["y"],self.store["w"],self.store["h"],px,py)
    in_rech =rect_contains(self.recharge["x"],self.recharge["y"],self.recharge["w"],self.recharge["h"],px,py)
    sx=self.store["x"]+self.store["w"]/2; sy=self.store["y"]+self.store["h"]/2
    rx=self.recharge["x"]+self.recharge["w"]/2; ry=self.recharge["y"]+self.recharge["h"]/2
    return {"self":{"x":px,"y":py,"health":mep.get("health",100.0),"energy":mep.get("energy",100.0),
                    "coins":mep.get("coins",0),"food":mep.get("food",0)},
            "coin":coin,"in_store":in_store,"in_recharge":in_rech,
            "store_c":(sx,sy),"rech_c":(rx,ry),"crowd":{"friends":fr,"foes":fo}}

  def _disc(self,obs):
    px=int(obs["self"]["x"]//128); py=int(obs["self"]["y"]//128)
    fr=obs["crowd"]["friends"]; fo=obs["crowd"]["foes"]; return (px,py,fr,fo)

  def _policy(self,a,name,p,obs,coins):
    px,py=p.get("x",0.0),p.get("y",0.0)
    E=p.get("energy",100.0); H=p.get("health",100.0); C=p.get("coins",0); F=p.get("food",0)
    speed=155.0; ux=uy=0.0; beh="idle"

    need_store = (C>=FOOD_PRICE and (H<85 or E<70)) or (F>0 and (H<80 or E<80))
    if E<15: a=11
    elif need_store: a=10
    elif coins and C<FOOD_PRICE: a=9

    if a==9 and coins:
      coin,_=self._nearest_coin(px,py,coins)
      if coin:
        dx,dy=unit_towards(px,py,coin["x"],coin["y"]); ux,uy=dx*speed,dy*speed; beh="seek_coin"
    elif a==10:
      sx,sy=obs["store_c"]; dx,dy=unit_towards(px,py,sx,sy); ux,uy=dx*speed,dy*speed; beh="go_store"
    elif a==11:
      rx,ry=obs["rech_c"]; dx,dy=unit_towards(px,py,rx,ry); ux,uy=dx*speed,dy*speed; beh="recharge"
    elif 1<=a<=8:
      dirs={1:(0,-1),2:(0,1),3:(-1,0),4:(1,0),5:(-1,-1),6:(1,-1),7:(-1,1),8:(1,1)}
      dx,dy=dirs[a]; ux,uy=dx*speed,dy*speed; beh="drift"
    elif a==14:
      ang=self.agents[name].rng.random()*6.2831853; ux,uy=math.cos(ang)*speed,math.sin(ang)*speed; beh="wander"
    return ux,uy,beh

  def _hud(self,name,p,beh):
    return (f"{name} | H:{int(p.get('health',0))} E:{int(p.get('energy',0))} "
            f"C:{int(p.get('coins',0))} F:{int(p.get('food',0))} "
            f"P:{int(p.get('perf',0))} Act:{beh}")

_BRAIN=Brain()
def api_init(cfg): return json.dumps(_BRAIN.api_init(cfg))
def api_tick(world): return json.dumps(_BRAIN.api_tick(world))
def api_reward(player,value,reason): return json.dumps(_BRAIN.api_reward(player,value,reason))
def api_save(): return json.dumps(_BRAIN.api_save())

