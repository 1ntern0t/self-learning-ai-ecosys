#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <GL/glew.h>
#include <GL/glu.h>
#include <Python.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <random>

struct Texture { GLuint id=0; int w=0,h=0; };
struct Rect { float x,y,w,h; };

struct Player {
  std::string name;
  float x=0,y=0, vx=0,vy=0;
  float health=100, energy=100;
  int coins=0, food=0;
  float intel=0, perf=0;
  int deaths=0;
  float speedBoostT=0.0f;
  std::string hud, intent;
  std::string status;
};

struct Coin { float x=0,y=0; };

enum class CrateType { Coins3, Food1, Speed8s, Heal30 };
struct Crate { float x=0,y=0; CrateType t=CrateType::Coins3; };

static const int WORLD_W=2048, WORLD_H=2048;
static const Rect STORE    ={0,0,360,360};
static const Rect RECHARGE ={WORLD_W-360.0f,0,360,360};

static float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static bool in_rect(const Rect& r,float x,float y){ return x>=r.x&&x<=r.x+r.w&&y>=r.y&&y<=r.y+r.h; }
static float dist2(float ax,float ay,float bx,float by){ float dx=ax-bx,dy=ay-by; return dx*dx+dy*dy; }

static std::optional<Texture> tex_from_surface(SDL_Surface* s){
  if(!s) return std::nullopt;
  GLenum fmt = s->format->BytesPerPixel==4?GL_RGBA:GL_RGB;
  Texture t;
  glGenTextures(1,&t.id);
  glBindTexture(GL_TEXTURE_2D,t.id);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D,0,fmt,s->w,s->h,0,fmt,GL_UNSIGNED_BYTE,s->pixels);
  glBindTexture(GL_TEXTURE_2D,0);
  t.w=s->w; t.h=s->h;
  return t;
}
static std::optional<Texture> load_texture(const std::string& path){
  SDL_Surface* s=IMG_Load(path.c_str());
  if(!s) return std::nullopt;
  SDL_Surface* c=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_ABGR8888,0);
  SDL_FreeSurface(s);
  if(!c) return std::nullopt;
  auto t=tex_from_surface(c);
  SDL_FreeSurface(c);
  return t;
}
static Texture text_to_texture(TTF_Font* font,const std::string& txt,SDL_Color color){
  Texture t;
  if(!font || txt.empty()) return t;
  SDL_Surface* s=TTF_RenderUTF8_Blended(font,txt.c_str(),color);
  if(!s) return t;
  SDL_Surface* c=SDL_ConvertSurfaceFormat(s,SDL_PIXELFORMAT_ABGR8888,0);
  SDL_FreeSurface(s);
  if(!c) return t;
  auto to=tex_from_surface(c);
  SDL_FreeSurface(c);
  if(to) t=*to;
  return t;
}
static void draw_textured_quad(const Texture& tex,float x,float y,float w,float h){
  if(!tex.id) return;
  glBindTexture(GL_TEXTURE_2D,tex.id);
  glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(x,y);
    glTexCoord2f(1,0); glVertex2f(x+w,y);
    glTexCoord2f(1,1); glVertex2f(x+w,y+h);
    glTexCoord2f(0,1); glVertex2f(x,y+h);
  glEnd();
  glBindTexture(GL_TEXTURE_2D,0);
}
static void draw_filled_rect(float x,float y,float w,float h,float r,float g,float b,float a){
  glDisable(GL_TEXTURE_2D);
  glColor4f(r,g,b,a);
  glBegin(GL_QUADS);
    glVertex2f(x,y); glVertex2f(x+w,y);
    glVertex2f(x+w,y+h); glVertex2f(x,y+h);
  glEnd();
  glColor4f(1,1,1,1);
  glEnable(GL_TEXTURE_2D);
}
static void begin_ortho(float left,float right,float bottom,float top){
  glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(left,right,bottom,top,-1,1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

static void draw_text_outlined(TTF_Font* font,const std::string& txt,
                               SDL_Color fg, SDL_Color outline, float x,float y){
  if(!font || txt.empty()) return;
  Texture t_fg=text_to_texture(font,txt,fg);
  Texture t_ol=text_to_texture(font,txt,outline);
  if(!t_fg.id || !t_ol.id){
    if(t_fg.id) glDeleteTextures(1,&t_fg.id);
    if(t_ol.id) glDeleteTextures(1,&t_ol.id);
    return;
  }
  glBindTexture(GL_TEXTURE_2D, t_ol.id);
  const float o=2.0f;
  for(int dx=-1; dx<=1; ++dx){
    for(int dy=-1; dy<=1; ++dy){
      if(dx==0 && dy==0) continue;
      float ox=x+dx*o, oy=y+dy*o;
      glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2f(ox,oy);
        glTexCoord2f(1,0); glVertex2f(ox+t_ol.w,oy);
        glTexCoord2f(1,1); glVertex2f(ox+t_ol.w,oy+t_ol.h);
        glTexCoord2f(0,1); glVertex2f(ox,oy+t_ol.h);
      glEnd();
    }
  }
  glBindTexture(GL_TEXTURE_2D, t_fg.id);
  glBegin(GL_QUADS);
    glTexCoord2f(0,0); glVertex2f(x,y);
    glTexCoord2f(1,0); glVertex2f(x+t_fg.w,y);
    glTexCoord2f(1,1); glVertex2f(x+t_fg.w,y+t_fg.h);
    glTexCoord2f(0,1); glVertex2f(x,y+t_fg.h);
  glEnd();
  glBindTexture(GL_TEXTURE_2D,0);
  glDeleteTextures(1,&t_fg.id);
  glDeleteTextures(1,&t_ol.id);
}

struct Camera { float cx=0, cy=0, scale=1.0f; };
static void screen_to_world(const Camera& cam,int w,int h,int mx,int my,float& wx,float& wy){
  float vw=WORLD_W/cam.scale, vh=WORLD_H/cam.scale;
  wx=cam.cx + (float(mx)/w)*vw; wy=cam.cy + (float(my)/h)*vh;
}
static void zoom_on_point(Camera& cam,float z,float ax,float ay){
  float before=cam.scale;
  cam.scale=clampf(cam.scale*z,0.25f,6.0f);
  float s=cam.scale/before;
  cam.cx=ax-(ax-cam.cx)/s; cam.cy=ay-(ay-cam.cy)/s;
  cam.cx=clampf(cam.cx,0,WORLD_W-WORLD_W/cam.scale);
  cam.cy=clampf(cam.cy,0,WORLD_H-WORLD_H/cam.scale);
}

struct PyBrain {
  PyObject* mod=nullptr,*api_init=nullptr,*api_tick=nullptr,*api_reward=nullptr,*api_save=nullptr;
  PyObject* json_mod=nullptr,*json_loads=nullptr;
  bool init(){
    Py_Initialize();
    if(!Py_IsInitialized()){ std::fprintf(stderr,"Py init fail\n"); return false; }
    PyObject* sys_path=PySys_GetObject((char*)"path");
    PyList_Append(sys_path,PyUnicode_FromString("."));
    mod=PyImport_ImportModule("brain");
    if(!mod){ PyErr_Print(); return false; }
    api_init=PyObject_GetAttrString(mod,"api_init");
    api_tick=PyObject_GetAttrString(mod,"api_tick");
    api_reward=PyObject_GetAttrString(mod,"api_reward");
    api_save=PyObject_GetAttrString(mod,"api_save");
    if(!api_init||!api_tick||!api_reward||!api_save){ PyErr_Print(); return false; }
    json_mod=PyImport_ImportModule("json");
    if(!json_mod){ PyErr_Print(); return false; }
    json_loads=PyObject_GetAttrString(json_mod,"loads");
    if(!json_loads){ PyErr_Print(); return false; }
    return true;
  }
  void shutdown(){
    if(api_save){
      PyObject* r=PyObject_CallFunction(api_save,nullptr);
      Py_XDECREF(r);
    }
    Py_XDECREF(json_loads); Py_XDECREF(json_mod);
    Py_XDECREF(api_save); Py_XDECREF(api_reward); Py_XDECREF(api_tick); Py_XDECREF(api_init);
    Py_XDECREF(mod);
    if(Py_IsInitialized()) Py_Finalize();
  }
  bool call_init(const std::vector<std::string>& names){
    std::ostringstream ss;
    ss<<"{\"bounds\":{\"w\":"<<WORLD_W<<",\"h\":"<<WORLD_H<<"},";
    ss<<"\"store\":{\"x\":0,\"y\":0,\"w\":360,\"h\":360},";
    ss<<"\"recharge\":{\"x\":"<<(WORLD_W-360)<<",\"y\":0,\"w\":360,\"h\":360},";
    ss<<"\"players\":[";
    for(size_t i=0;i<names.size();++i){
      if(i) ss<<",";
      ss<<"\""<<names[i]<<"\"";
    }
    ss<<"]}";
    PyObject* arg=Py_BuildValue("(s)", ss.str().c_str());
    PyObject* ret=PyObject_CallObject(api_init,arg);
    Py_DECREF(arg);
    if(!ret){ PyErr_Print(); return false; }
    Py_DECREF(ret);
    return true;
  }
  bool tick_and_get_decisions(
    int tick,float dt,const std::vector<Player>& players,const std::vector<Coin>& coins,
    std::unordered_map<std::string,std::string>& outHUD,
    std::unordered_map<std::string,std::pair<float,float>>& outVel,
    std::unordered_map<std::string,std::string>& outIntent){
    std::ostringstream ss;
    ss<<"{\"tick\":"<<tick<<",\"dt\":"<<dt<<",\"bounds\":{\"w\":"<<WORLD_W<<",\"h\":"<<WORLD_H<<"},";
    ss<<"\"store\":{\"x\":0,\"y\":0,\"w\":360,\"h\":360},";
    ss<<"\"recharge\":{\"x\":"<<(WORLD_W-360)<<",\"y\":0,\"w\":360,\"h\":360},";
    ss<<"\"coins\":[";
    for(size_t i=0;i<coins.size();++i){
      if(i) ss<<",";
      ss<<"{\"x\":"<<coins[i].x<<",\"y\":"<<coins[i].y<<"}";
    }
    ss<<"],\"players\":{";
    for(size_t i=0;i<players.size();++i){
      const auto& p=players[i];
      if(i) ss<<",";
      ss<<"\""<<p.name<<"\":{";
      ss<<"\"x\":"<<p.x<<",\"y\":"<<p.y<<",\"vx\":"<<p.vx<<",\"vy\":"<<p.vy<<",";
      ss<<"\"health\":"<<p.health<<",\"energy\":"<<p.energy<<",";
      ss<<"\"coins\":"<<p.coins<<",\"food\":"<<p.food<<",";
      ss<<"\"intel\":"<<p.intel<<",\"perf\":"<<p.perf<<"}";
    }
    ss<<"}}";
    PyObject* arg=Py_BuildValue("(s)", ss.str().c_str());
    PyObject* retStr=PyObject_CallObject(api_tick,arg);
    Py_DECREF(arg);
    if(!retStr){ PyErr_Print(); return false; }
    PyObject* parsed=PyObject_CallFunction(json_loads,"O",retStr);
    Py_DECREF(retStr);
    if(!parsed){ PyErr_Print(); return false; }
    PyObject* ok=PyDict_GetItemString(parsed,"ok");
    if(!ok||!PyObject_IsTrue(ok)){ Py_DECREF(parsed); return false; }
    PyObject* decisions=PyDict_GetItemString(parsed,"decisions");
    if(!decisions||!PyDict_Check(decisions)){ Py_DECREF(parsed); return false; }
    PyObject *k,*v; Py_ssize_t pos=0;
    while(PyDict_Next(decisions,&pos,&k,&v)){
      std::string name = PyUnicode_Check(k)? std::string(PyUnicode_AsUTF8(k)) : "";
      if(name.empty()) continue;
      PyObject* vx=PyDict_GetItemString(v,"vx");
      PyObject* vy=PyDict_GetItemString(v,"vy");
      PyObject* hud=PyDict_GetItemString(v,"hud");
      PyObject* intent=PyDict_GetItemString(v,"intent");
      outVel[name] = { vx? (float)PyFloat_AsDouble(vx):0.0f, vy? (float)PyFloat_AsDouble(vy):0.0f };
      outHUD[name] = hud? std::string(PyUnicode_AsUTF8(hud)) : "";
      outIntent[name] = intent? std::string(PyUnicode_AsUTF8(intent)) : "";
    }
    Py_DECREF(parsed);
    return true;
  }
  void reward(const std::string& player,double value,const std::string& reason){
    PyObject* r=PyObject_CallFunction(api_reward,"sds",player.c_str(),value,reason.c_str());
    Py_XDECREF(r);
  }
};

int main(int,char**){
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ std::fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
  if(IMG_Init(IMG_INIT_PNG)==0){ std::fprintf(stderr,"IMG_Init: %s\n",IMG_GetError()); return 1; }
  if(TTF_Init()!=0){ std::fprintf(stderr,"TTF_Init: %s\n",TTF_GetError()); return 1; }

  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);

  int winW=1440, winH=900;
  SDL_Window* win=SDL_CreateWindow("AI Player EcoSys - Zone Labels + Mystery Crates",
    SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,winW,winH,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
  if(!win){ std::fprintf(stderr,"CreateWindow: %s\n",SDL_GetError()); return 1; }
  SDL_GLContext glctx=SDL_GL_CreateContext(win);
  if(!glctx){ std::fprintf(stderr,"GL ctx fail\n"); return 1; }
  glewInit();
  glViewport(0,0,winW,winH);
  glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

  auto playerTex=load_texture("images/player.png");
  auto coinTex  =load_texture("images/coin.png");
  if(!playerTex) std::fprintf(stderr,"Missing images/player.png\n");
  if(!coinTex)   std::fprintf(stderr,"Missing images/coin.png\n");

  TTF_Font* font=TTF_OpenFont("DejaVuSans.ttf",20);
  TTF_Font* fontSmall=TTF_OpenFont("DejaVuSans.ttf",19);
  if(!font||!fontSmall) std::fprintf(stderr,"TTF_OpenFont failed: %s\n",TTF_GetError());
  SDL_Color hudColor={255,255,255,255};
  SDL_Color neonGreen={80,255,120,255};
  SDL_Color neonTitle={0,255,60,255};
  SDL_Color outlineCol={0,0,0,255}; 
  SDL_Color zoneLabel={255,255,255,255};

  std::vector<Player> players; players.reserve(25);
  for(int i=0;i<25;++i){
    Player p; p.name="Player"+std::to_string(i+1);
    int cols=5; float gx=(i%cols), gy=(i/cols);
    p.x=200.0f + gx*((WORLD_W-400.0f)/(cols-1));
    p.y=300.0f + gy*((WORLD_H-600.0f)/((25/cols)-1 + ((25%cols)?1:0)));
    players.push_back(p);
  }
  std::vector<Coin> coins;
  std::vector<Crate> crates;

  std::mt19937 rng(1337);
  auto randf=[&](float a,float b){ std::uniform_real_distribution<float> d(a,b); return d(rng); };
  auto randi=[&](int a,int b){ std::uniform_int_distribution<int> d(a,b); return d(rng); };
  double crateSpawnTimer = 10.0; 

  PyBrain brain;
  if(!brain.init()){ std::fprintf(stderr,"Python bridge init failed\n"); return 1; }
  { std::vector<std::string> names; names.reserve(players.size());
    for(auto& p:players) names.push_back(p.name);
    brain.call_init(names);
  }

  Camera cam; cam.scale=0.75f;

  bool running=true, rightDragging=false, showStatsPanel=true;
  int lastMouseX=0,lastMouseY=0, mouseX=0,mouseY=0;
  Uint64 prev=SDL_GetPerformanceCounter(); double acc=0.0; const double dt=1.0/60.0; int tick=0;

  auto gain_intel=[&](Player& p,float amount){ p.intel=clampf(p.intel+amount,0.0f,100.0f); };

  auto apply_transactions=[&](Player& p){
    if(in_rect(STORE,p.x,p.y)){
      int reserve = std::min(3, 1 + (int)std::floor(p.intel / 40.0f));
      if(p.coins>=5 && p.food < reserve){
        p.coins -= 5; p.food += 1; gain_intel(p, 0.5f);
        brain.reward(p.name, +0.8, "buy_food_reserve");
      }
      bool lowHealth = (p.health <= 70.0f);
      bool lowEnergy = (p.energy <= 60.0f);
      if(p.food>0 && (lowHealth || lowEnergy)){
        p.food -= 1;
        p.health = clampf(p.health + 25.0f, 0, 100);
        p.energy = clampf(p.energy + 20.0f, 0, 100);
        gain_intel(p, 0.5f);
        brain.reward(p.name, +1.0, "eat_food_delayed");
      }
    }
    if(in_rect(RECHARGE,p.x,p.y)){
      float before=p.energy;
      p.energy=clampf(p.energy + 30.0f*(float)dt, 0, 100);
      if(p.energy>before) brain.reward(p.name, +0.2, "recharge");
    }
    if(p.food >= 1 && p.health > 70.0f && p.energy > 60.0f){
      brain.reward(p.name, +0.02, "maintain_food_reserve");
    }
  };

  auto collect_coins=[&](Player& p){
    for(size_t i=0;i<coins.size();){
      if(dist2(p.x,p.y,coins[i].x,coins[i].y) <= 40.0f*40.0f){
        coins.erase(coins.begin()+i);
        p.coins += 1; p.perf+=0.5f; gain_intel(p, 0.25f);
        brain.reward(p.name, +1.0, "collect_coin");
      } else {
        ++i;
      }
    }
  };

  auto collect_crates=[&](Player& p){
    for(size_t i=0;i<crates.size();){
      if(dist2(p.x,p.y,crates[i].x,crates[i].y) <= 45.0f*45.0f){
        CrateType t = crates[i].t;
        crates.erase(crates.begin()+i);
        switch(t){
          case CrateType::Coins3:
            p.coins += 3; p.perf += 1.0f; p.status="CRATE: +3 coins";
            brain.reward(p.name, +1.2, "crate_coins3");
            break;
          case CrateType::Food1:
            p.food = std::min(p.food+1, 9); p.perf += 0.8f; p.status="CRATE: +1 food";
            brain.reward(p.name, +1.0, "crate_food1");
            break;
          case CrateType::Speed8s:
            p.speedBoostT = std::max(p.speedBoostT, 8.0f); p.perf += 0.8f; p.status="CRATE: speed x1.5 (8s)";
            brain.reward(p.name, +0.8, "crate_speed");
            break;
          case CrateType::Heal30:
            p.health = clampf(p.health + 30.0f, 0, 100); p.perf += 0.8f; p.status="CRATE: +30 health";
            brain.reward(p.name, +0.8, "crate_heal30");
            break;
        }
      } else {
        ++i;
      }
    }
  };

  auto player_rect=[&](const Player& p)->Rect{ return Rect{p.x-35.0f,p.y-60.0f,70.0f,120.0f}; };
  auto mouse_over_player=[&](const Player& p)->bool{
    float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy); Rect r=player_rect(p);
    return wx>=r.x&&wx<=r.x+r.w&&wy>=r.y&&wy<=r.y+r.h;
  };

  while(running){
    SDL_Event e;
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT) running=false;
      if(e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_SIZE_CHANGED){
        winW=e.window.data1; winH=e.window.data2; glViewport(0,0,winW,winH);
      }
      if(e.type==SDL_MOUSEMOTION){
        mouseX=e.motion.x; mouseY=e.motion.y;
        if(rightDragging){
          int dx=e.motion.x-lastMouseX, dy=e.motion.y-lastMouseY;
          lastMouseX=e.motion.x; lastMouseY=e.motion.y;
          float vw=WORLD_W/cam.scale, vh=WORLD_H/cam.scale;
          cam.cx=clampf(cam.cx - dx*(vw/winW), 0, WORLD_W - vw);
          cam.cy=clampf(cam.cy - dy*(vh/winH), 0, WORLD_H - vh);
        }
      }
      if(e.type==SDL_KEYDOWN){
        auto k=e.key.keysym.sym;
        if(k==SDLK_ESCAPE||k==SDLK_q) running=false;
        if(k==SDLK_F1) showStatsPanel=!showStatsPanel;
        if(k==SDLK_0){ cam.scale=0.75f; cam.cx=0; cam.cy=0; }
        if(k==SDLK_PLUS||k==SDLK_EQUALS){
          float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy);
          zoom_on_point(cam,1.1f,wx,wy);
        }
        if(k==SDLK_MINUS||k==SDLK_UNDERSCORE){
          float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy);
          zoom_on_point(cam,0.9f,wx,wy);
        }
        if(k==SDLK_s){
          float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy);
          Crate c; c.x=clampf(wx,20,WORLD_W-20); c.y=clampf(wy,20,WORLD_H-20);
          int pick=randi(0,3);
          c.t = pick==0?CrateType::Coins3 : pick==1?CrateType::Food1 : pick==2?CrateType::Speed8s : CrateType::Heal30;
          crates.push_back(c);
        }
      }
      if(e.type==SDL_MOUSEWHEEL){
        float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy);
        if(e.wheel.y>0) zoom_on_point(cam,1.1f,wx,wy);
        if(e.wheel.y<0) zoom_on_point(cam,0.9f,wx,wy);
      }
      if(e.type==SDL_MOUSEBUTTONDOWN){
        if(e.button.button==SDL_BUTTON_RIGHT){
          rightDragging=true; lastMouseX=e.button.x; lastMouseY=e.button.y;
        }
        if(e.button.button==SDL_BUTTON_LEFT){
          float wx,wy; screen_to_world(cam,winW,winH,e.button.x,e.button.y,wx,wy);
          wx=clampf(wx,10,WORLD_W-10); wy=clampf(wy,10,WORLD_H-10);
          coins.push_back({wx,wy});
        }
      }
      if(e.type==SDL_MOUSEBUTTONUP){
        if(e.button.button==SDL_BUTTON_RIGHT) rightDragging=false;
      }
    }

    Uint64 now=SDL_GetPerformanceCounter();
    double elapsed=(double)(now-prev)/(double)SDL_GetPerformanceFrequency();
    if(elapsed>0.25) elapsed=0.25;
    prev=now; acc+=elapsed;

    crateSpawnTimer -= elapsed;
    if(crateSpawnTimer<=0.0){
      Crate c; c.x=randf(60.0f, WORLD_W-60.0f); c.y=randf(100.0f, WORLD_H-60.0f);
      int pick=randi(0,3);
      c.t = pick==0?CrateType::Coins3 : pick==1?CrateType::Food1 : pick==2?CrateType::Speed8s : CrateType::Heal30;
      crates.push_back(c);
      crateSpawnTimer = randf(12.0f, 22.0f);
    }

    while(acc>=dt){
      ++tick;

      std::unordered_map<std::string,std::string> hudMap,intentMap;
      std::unordered_map<std::string,std::pair<float,float>> velMap;
      brain.tick_and_get_decisions(tick,(float)dt,players,coins,hudMap,velMap,intentMap);

      for(auto& p:players){
        auto it=velMap.find(p.name);
        float vx = (it!=velMap.end())? it->second.first : 0.0f;
        float vy = (it!=velMap.end())? it->second.second: 0.0f;
        float boost = (p.speedBoostT>0.0f ? 1.5f : 1.0f);
        p.vx = vx*boost; p.vy = vy*boost;
      }

      const float sepRadius=80.0f, sepRadius2=sepRadius*sepRadius;
      const float sepStrength=320.0f, maxSpeed=220.0f, maxAccel=600.0f;
      for(size_t i=0;i<players.size();++i){
        float ax=0, ay=0;
        for(size_t j=0;j<players.size();++j){
          if(i==j) continue;
          float d2=dist2(players[i].x,players[i].y,players[j].x,players[j].y);
          if(d2<sepRadius2 && d2>1.0f){
            float d=std::sqrt(d2);
            float nx=(players[i].x-players[j].x)/d, ny=(players[i].y-players[j].y)/d;
            float w=(sepRadius - d)/sepRadius;
            ax += nx*sepStrength*w; ay += ny*sepStrength*w;
            brain.reward(players[i].name, -0.02*w, "too_close");
          }
        }
        float alen=std::sqrt(ax*ax+ay*ay);
        if(alen>maxAccel){ ax*=maxAccel/alen; ay*=maxAccel/alen; }
        players[i].vx += ax*(float)dt; players[i].vy += ay*(float)dt;
        float vlen=std::sqrt(players[i].vx*players[i].vx+players[i].vy*players[i].vy);
        if(vlen>maxSpeed){ players[i].vx*=maxSpeed/vlen; players[i].vy*=maxSpeed/vlen; }
      }

      for(auto& p:players){
        if(p.speedBoostT>0.0f) p.speedBoostT = std::max(0.0f, p.speedBoostT - (float)dt);

        p.x += p.vx*(float)dt; p.y += p.vy*(float)dt;
        p.x = clampf(p.x,0,WORLD_W); p.y = clampf(p.y,0,WORLD_H);

        float baseDrain=4.0f;
        float staminaFactor = 1.0f - 0.12f * (p.intel/100.0f);
        staminaFactor = clampf(staminaFactor,0.7f,1.0f);
        float eDrain = baseDrain * staminaFactor;

        p.energy = clampf(p.energy - eDrain*(float)dt, 0, 100);
        p.health = clampf(p.health - ((p.energy<=0)?6.0f:1.0f)*(float)dt, 0, 100);

        if(p.health<=0){
          p.deaths += 1;
          p.x=1024; p.y=1024; p.vx=0; p.vy=0;
          p.health=100; p.energy=60;
          p.coins=std::max(0,p.coins-1);
          p.status.clear();
          brain.reward(p.name, -2.0, "death");
        }

        apply_transactions(p);
        collect_coins(p);
        collect_crates(p);

        auto h=hudMap.find(p.name); p.hud = (h!=hudMap.end())? h->second : (p.name+" | ...");
        auto it=intentMap.find(p.name); p.intent = (it!=intentMap.end())? it->second : "";
      }

      acc-=dt;
    }

    glClearColor(0.05f,0.06f,0.08f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    float vw=WORLD_W/cam.scale, vh=WORLD_H/cam.scale;
    begin_ortho(cam.cx,cam.cx+vw,cam.cy+vh,cam.cy);

    draw_filled_rect(0,0,WORLD_W,WORLD_H, 0.10f,0.11f,0.13f,1.0f);
    draw_filled_rect(STORE.x,STORE.y,STORE.w,STORE.h, 0.15f,0.35f,0.20f,0.45f);
    draw_filled_rect(RECHARGE.x,RECHARGE.y,RECHARGE.w,RECHARGE.h, 0.20f,0.25f,0.55f,0.45f);

    if(font){
      draw_text_outlined(font, "STORE", zoneLabel, outlineCol,
                         STORE.x + STORE.w*0.5f - 40.0f, STORE.y + 18.0f);
      draw_text_outlined(font, "RECHARGE", zoneLabel, outlineCol,
                         RECHARGE.x + 12.0f, RECHARGE.y + 18.0f);
    }

    if(coinTex){
      for(const auto& c:coins) draw_textured_quad(*coinTex,c.x-17.5f,c.y-17.5f,35.0f,35.0f);
    } else {
      for(const auto& c:coins) draw_filled_rect(c.x-8,c.y-8,16,16,0.9f,0.8f,0.1f,1.0f);
    }

    for(const auto& cr:crates){
      draw_filled_rect(cr.x-18, cr.y-18, 36, 36, 0.68f, 0.35f, 0.85f, 0.95f);
    }

    for(const auto& p:players){
      if(playerTex) draw_textured_quad(*playerTex,p.x-35.0f,p.y-60.0f,70.0f,120.0f);
      else draw_filled_rect(p.x-15,p.y-25,30,50,0.8f,0.2f,0.2f,1.0f);
      if(!p.hud.empty() && font){
        float wx,wy; screen_to_world(cam,winW,winH,mouseX,mouseY,wx,wy);
        if(wx>=p.x-35.0f && wx<=p.x+35.0f && wy>=p.y-60.0f && wy<=p.y+60.0f){
          std::string hud = p.hud;
          if(!p.status.empty()) hud += " [" + p.status + "]";
          Texture t=text_to_texture(font,hud,hudColor);
          if(t.id){
            draw_textured_quad(t,p.x - t.w*0.5f, p.y-85.0f, (float)t.w,(float)t.h);
            glDeleteTextures(1,&t.id);
          }
        }
      }
    }

    begin_ortho(0,(float)winW,(float)winH,0);
    if(showStatsPanel && fontSmall){
      float panelW=610.0f, panelH=(float)winH-40.0f, panelX=20.0f, panelY=20.0f;
      draw_filled_rect(panelX,panelY,panelW,panelH, 0.03f,0.03f,0.03f,0.92f);

      draw_text_outlined(font, "Self-Learning AI EcoSys - Player Stats (F1)", neonTitle, outlineCol, panelX+12, panelY+10);
      draw_text_outlined(fontSmall, "Rank  Name        H   E   C   F   IQ   P    D   Act / Status",
                         neonGreen, outlineCol, panelX+12, panelY+44);

      std::vector<const Player*> order; order.reserve(players.size());
      for(const auto& p:players) order.push_back(&p);
      std::sort(order.begin(),order.end(),[](const Player* a,const Player* b){ return a->perf>b->perf; });

      float y=panelY+70.0f;
      for(size_t i=0;i<order.size();++i){
        const Player* p=order[i];
        std::ostringstream line; line.setf(std::ios::fixed); line.precision(0);
        line<<(i+1<10?"  ":" ")<<(i+1)<<".  "
            <<p->name<<(p->name.size()<8? std::string(8-p->name.size(),' ') : " ")
            <<" "<<p->health
            <<"  "<<p->energy
            <<"  "<<p->coins
            <<"  "<<p->food
            <<"  "<<p->intel
            <<"  "<<p->perf
            <<"   "<<p->deaths
            <<"   "<<(p->intent.empty()?"-":p->intent);
        if(!p->status.empty()) line<<"  ["<<p->status<<"]";
        draw_text_outlined(fontSmall, line.str(), neonGreen, outlineCol, panelX+12, y);
        y+=22.0f;
        if(y>panelY+panelH-24.0f) break;
      }
    }

    SDL_GL_SwapWindow(win);
  }

  brain.shutdown();
  if(fontSmall) TTF_CloseFont(fontSmall);
  if(font) TTF_CloseFont(font);
  SDL_GL_DeleteContext(glctx);
  SDL_DestroyWindow(win);
  TTF_Quit(); IMG_Quit(); SDL_Quit();
  return 0;
}

