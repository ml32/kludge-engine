/* included by renderer-gl3.c -- do not compile separately */

#define DEF_BLOCK_SCENE \
"layout(std140) uniform scene {\n"\
"  uniform mat4 viewmatrix;\n"\
"  uniform mat4 projmatrix;\n"\
"  uniform mat4 iprojmatrix;\n"\
"  uniform mat4 vpmatrix;\n"\
"  uniform vec3 viewpos;\n"\
"  uniform mat3 viewrot;\n"\
"  uniform vec4 viewport;\n"\
"  uniform float near;\n"\
"  uniform float far;\n"\
"};\n"

#define DEF_NORMAL_ENCODING \
"vec2 encode_normal(vec3 n) {\n"\
"  n = normalize(n);\n"\
"  float scale = sqrt(2.0/(1.0 + n.z));\n"\
"  return n.xy * scale * 0.5 + 0.5;\n"\
"}\n"\
"vec3 decode_normal(vec2 xy) {\n"\
"  xy = xy * 2.0 - 1.0;\n"\
"  float lensq = dot(xy, xy);\n"\
"  float xyscale = sqrt(1.0 - lensq / 4.0);\n"\
"  float zscale  = lensq / 2.0 - 1.0;\n"\
"  return normalize(vec3(xy * xyscale, -zscale));\n"\
"}\n"

static const char *vshader_gbuffer_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out float fdepth;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  fdepth    = -(viewmatrix * vec4(vposition, 1.0)).z;\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = viewrot * mat3(normalize(vtangent.xyz), normalize(vbitangent), normalize(vnormal));\n"
"\n"
"  gl_Position = vpmatrix * vec4(vposition, 1.0);\n"
"}\n";

static const char *fshader_gbuffer_src =
"#version 330\n"
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"uniform sampler2D tspecular;\n"
"uniform sampler2D temissive;\n"
"smooth in float fdepth;\n"
"smooth in vec2  ftexcoord;\n"
"smooth in mat3  tbnmatrix;\n"
"layout(location = 0) out float gdepth;\n"
"layout(location = 1) out vec2  gnormal;\n"
"layout(location = 2) out vec4  gdiffuse;\n"
"layout(location = 3) out vec4  gspecular;\n"
"layout(location = 4) out vec4  gemissive;\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  vec4 diffuse = texture(tdiffuse, ftexcoord);\n"
"  if (diffuse.a < 0.5) discard;\n"
"\n"
"  vec3 n_local = texture(tnormal, ftexcoord).xyz * 2.0 - 1.0;\n"
"  vec3 n_world = normalize(tbnmatrix * n_local);\n"
"\n"
"  gdepth    = fdepth;\n"
"  gdiffuse  = diffuse;\n"
"  gnormal   = encode_normal(n_world);\n"
"  gspecular = texture(tspecular, ftexcoord);\n"
"  gemissive = texture(temissive, ftexcoord);\n"
"}\n";

static const char *vshader_downsample_src =
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_downsample_src =
"#version 330\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"layout(location = 0) out float gdepth;\n"
"layout(location = 1) out vec2  gnormal;\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  vec2 coord[4] = vec2[](\n"
"    gl_FragCoord.xy * 2.0 + vec2(-0.5, -0.5),\n"
"    gl_FragCoord.xy * 2.0 + vec2( 0.5, -0.5),\n"
"    gl_FragCoord.xy * 2.0 + vec2(-0.5,  0.5),\n"
"    gl_FragCoord.xy * 2.0 + vec2( 0.5,  0.5)\n"
"  );\n"
"  float depth  = 0.0;\n"
"  vec3  normal = vec3(0.0, 0.0, 0.0);\n"
"  for (int i=0; i < 4; i++) {\n"
"    depth += texture(tdepth, coord[i]).r;\n"
"    vec3 n = decode_normal(texture(tnormal, coord[i]).rg);\n"
"    normal += n;\n"
"  }\n"
"  gdepth  = depth / 4.0;\n"
"  gnormal = encode_normal(normalize(normal));\n"
"}\n";

static const char *vshader_ssao_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"void main () {\n"
"  fray_eye    = vray_eye;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_ssao_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2D tnoise;\n"
"smooth in vec3 fray_eye;\n"
"layout(location = 0) out float ssao;\n"
DEF_NORMAL_ENCODING
"float calc_ao(vec3 offset, vec3 normal) {\n"
"  vec3  dir  = normalize(offset);\n"
"  float dist = length(offset);\n"
"  float falloff = 1.0 / (1.0 + dist * dist / 256.0);\n"
"  float ao = max(0.0, dot(dir, normal));\n"
"  return ao * falloff;\n"
"}\n"
"void main() {\n"
"  float depth  = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord  = fray_eye * depth;\n"
"  vec3  normal = decode_normal(texture(tnormal, gl_FragCoord.xy).rg);\n"
""
"  float occlusion = 0.0;\n"
"  for (int i=0; i < 25; i++) {\n"
"    vec2  offset = 4.0 * normalize(texture(tnoise, (gl_FragCoord.xy + vec2(1 << (i / 5), 1 << (i % 5))) / 256.0).xyz * 2.0 - 1.0).xy;\n"
"    float occluder_depth  = texture(tdepth, gl_FragCoord.xy + offset).r;\n"
"    vec3  occluder_coord  = fray_eye * occluder_depth;\n"
"    vec3  occluder_offset = occluder_coord - coord;\n" /* offset in eye-space */
"    occlusion += calc_ao(occluder_offset, normal);\n"
"  };\n"
"  ssao = occlusion / (25.0 * 6.0);\n"
"}\n";

static const char *vshader_upsample_src =
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_upsample_src =
"#version 330\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tsdepth;\n"
"uniform sampler2DRect tsnormal;\n"
"uniform sampler2DRect tocclusion;\n"
"layout(location = 0) out float ssao;\n"
DEF_NORMAL_ENCODING
"void main() {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3 normal = decode_normal(texture(tnormal, gl_FragCoord.xy).rg);\n"
"  float total = 0.0;\n"
"  float value = 0.0;\n"
"  for (int i=0; i < 4; i++) {\n"
"      vec2 offset   = vec2((i/2) - 0.5, (i%2) - 0.5);\n"
"      vec2 texcoord = gl_FragCoord.xy / 2.0 + offset;\n"
"      float sample_depth = texture(tsdepth, texcoord).r;\n"
"      vec3 sample_normal = decode_normal(texture(tsnormal, texcoord).rg);\n"
"      float depthweight = abs(sample_depth - depth) > 16.0 ? 0.0 : 1.0;\n"
"      float normweight  = pow(max(0.0, dot(normal, sample_normal)), 2.0);\n"
"      float scale = depthweight * normweight;\n"
"      value += scale * texture(tocclusion, texcoord).r;\n"
"      total += scale;\n"
"  }\n"
"  ssao = total < 0.01 ? 0.0 : value / total;\n"
"}\n";

static const char *vshader_minimal_src = 
"#version 330\n"
"uniform mat4 mvpmatrix;\n"
"layout(location = 0) in vec3 vcoord;\n"
"void main () {\n"
"  gl_Position  = mvpmatrix * vec4(vcoord, 1.0);\n"
"}\n";

static const char *fshader_flatcolor_src = 
"#version 330\n"
"uniform vec3 color;\n"
"layout(location = 0) out vec4 outcolor;\n"
"void main() {\n"
"  outcolor = vec4(color, 1.0);\n"
"}\n";

static const char *vshader_pointlight_src = 
"#version 330\n"
DEF_BLOCK_SCENE
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray;\n"
"smooth out vec3 flightpos;\n"
"void main () {\n"
"  fray      = vray_eye;\n"
"  flightpos = (viewmatrix * light.position).xyz;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_pointlight_src =
"#version 330\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tdiffuse;\n"
"uniform sampler2DRect tspecular;\n"
"smooth in vec3 fray;\n"
"smooth in vec3 flightpos;\n"
"layout(location = 0) out vec4 color;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord = fray * depth;\n"
"  float dist  = distance(flightpos, coord);\n"
"  float luminance = light.color.a / (dist * dist);\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  vec3 L = normalize(flightpos - coord);\n"
"  vec3 V = -normalize(fray);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R  = 2.0 * N * dot(N, L) - L;\n"
"  float fresnel = 0.25 + 0.75 * pow(1.0 - dot(V, H), 5.0);\n"
""
"  vec3 diff  = texture(tdiffuse, gl_FragCoord.xy).rgb;\n"
"  vec4 spec  = texture(tspecular, gl_FragCoord.xy);\n"
"  color.rgb  = diff * max(0.0, dot(N, L));\n" /* diffuse */
"  color.rgb += spec.rgb * fresnel * pow(max(0.0, dot(R, V)), spec.a*255.0);\n" /* specular */
"  color.rgb *= light.color.rgb * luminance;\n"
"  color.a    = 1.0;\n"
"}\n";

static const char *vshader_envlight_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"layout(location = 1) in vec3 vray_eye;\n"
"layout(location = 2) in vec3 vray_world;\n"
"smooth out vec3 fray_eye;\n"
"smooth out vec3 fray_world;\n"
"void main () {\n"
"  fray_eye    = vray_eye;\n"
"  fray_world  = vray_world;\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_envlight_src =
"#version 330\n"
DEF_BLOCK_SCENE
"layout(std140) uniform envlight {\n"
"  vec4  ambient;\n"
"  vec4  color;\n"
"  vec4  direction;\n"
"} light;\n"
"uniform sampler2DRect tdepth;\n"
"uniform sampler2DRect tnormal;\n"
"uniform sampler2DRect tdiffuse;\n"
"uniform sampler2DRect tspecular;\n"
"uniform sampler2DRect temissive;\n"
"uniform sampler2DRect tocclusion;\n"
"smooth in vec3 fray_eye;\n"
"smooth in vec3 fray_world;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
DEF_NORMAL_ENCODING
"void main () {\n"
"  float depth = texture(tdepth, gl_FragCoord.xy).r;\n"
"  vec3  coord_eye = fray_eye * depth;\n"
"  vec3  coord_world = fray_world * depth + viewpos;\n"
""
"  vec3 N = decode_normal(texture(tnormal, gl_FragCoord.xy).xy);\n"
""
"  float occlusion = pow(1.0 - max(0.0, texture(tocclusion, gl_FragCoord.xy).r), 2.0);\n"
""
"  vec3 L = normalize(viewrot * -light.direction.xyz);\n"
"  vec3 V = -normalize(fray_eye);\n"
"  vec3 H = normalize(L + V);\n"
"  vec3 R = 2.0 * N * dot(N, L) - L;\n"
"  float fresnel = 0.25 + 0.75 * pow(1.0 - dot(V, H), 5.0);\n"
""
"  vec3 diff  = texture(tdiffuse, gl_FragCoord.xy).rgb;\n"
"  vec4 spec  = texture(tspecular, gl_FragCoord.xy);\n"
"  vec4 glow  = texture(temissive, gl_FragCoord.xy);\n"
"  color.rgb += diff * max(0.0, dot(N, L));\n" /* diffuse */
"  color.rgb += spec.rgb * fresnel * pow(max(0.0, dot(R, V)), spec.a*255.0);\n" /* specular */
"  color.rgb *= light.color.rgb * light.color.a;\n" /* diffuse/specular scale */
"  color.rgb += diff * light.ambient.rgb * light.ambient.a * occlusion;\n" /* ambient */
"  color.rgb += glow.rgb * exp2(glow.a * 16.0 - 8.0);\n" /* emissive */
"  color.a    = 1.0;\n"
"}\n";

static const char *vshader_tonemap_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"void main () {\n"
"  gl_Position = vec4(vcoord, 0.0, 1.0);\n"
"}\n";

static const char *fshader_tonemap_src = 
"#version 330\n"
"uniform sampler2DRect tcomposite;\n"
"layout(location = 0) out vec4 color;\n"
"vec3 tonemap(vec3 color, float sigma) {\n"
"  color     = max(vec3(0.0, 0.0, 0.0), color);\n"
"  vec3  c2  = color * color;\n"
"  float var = sigma * sigma;\n"
"  return c2/(c2+var);\n"
"}\n"
"void main() {\n"
"  vec3 c = texture(tcomposite, gl_FragCoord.xy).rgb;\n"
"  color = vec4(tonemap(c, 0.5), 1.0);\n"
"}\n";

static const char *vshader_blit_src =
"#version 330\n"
"uniform vec2 size;\n"
"uniform vec2 offset;\n"
"layout(location = 0) in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord  = (vcoord * 0.5 + 0.5);\n"
"  gl_Position = vec4(vcoord * size + offset, 0.0, 1.0);\n"
"}\n";

static const char *fshader_blit_src = 
"#version 330\n"
"uniform float colorscale;\n"
"uniform float coloroffset;\n"
"uniform sampler2DRect image;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"void main() {\n"
"  color = texture(image, ftexcoord * textureSize(image)) * colorscale + coloroffset;\n"
"}\n";
