/* included by renderer-gl3.c -- do not compile separately */
static const char *vshader_gbuffer_src =
"#version 330\n"
"uniform mat4 vmatrix;\n"
"uniform mat4 vpmatrix;\n"
"layout(location = 0) in vec3 vposition;\n"
"layout(location = 1) in vec2 vtexcoord;\n"
"layout(location = 2) in vec3 vnormal;\n"
"layout(location = 3) in vec4 vtangent;\n"
"smooth out float fdepth;\n"
"smooth out vec2 ftexcoord;\n"
"smooth out mat3 tbnmatrix;\n"
"void main() {\n"
"  fdepth    = -(vmatrix * vec4(vposition, 1)).z;\n"
"  ftexcoord = vtexcoord;\n"
"\n"
"  vec3 vbitangent = cross(vnormal, vtangent.xyz) * vtangent.w;\n"
"  tbnmatrix = mat3(vmatrix[0].xyz, vmatrix[1].xyz, vmatrix[2].xyz) * mat3(vtangent.xyz, vbitangent, vnormal);\n"
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
"layout(location = 1) out vec4  gdiffuse;\n"
"layout(location = 2) out vec2  gnormal;\n"
"layout(location = 3) out vec4  gspecular;\n"
"layout(location = 4) out vec4  gemissive;\n"
"void main() {\n"
"  vec3 n_local = texture(tnormal, ftexcoord).xyz * 2.0 - 1.0;\n"
"  vec3 n_world = normalize(tbnmatrix * n_local);\n" /* this is actually eye-space, not world-space */
"\n"
"  gdepth    = fdepth;\n"
"  gdiffuse  = texture(tdiffuse, ftexcoord);\n"
"  gnormal   = n_world.xy;\n"
"  gspecular = texture(tspecular, ftexcoord);\n"
"  gemissive = texture(temissive, ftexcoord);\n"
"}\n";

static const char *vshader_pointlight_src = 
"#version 330\n"
"uniform mat4 vpmatrix;\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"layout(location = 0) in vec3 vcoord;\n"
"void main () {\n"
"  float intensity = light.color.a;\n"
"  float radius = 16.0 * sqrt(intensity);\n"
"  vec4  world  = vec4(vcoord * radius + light.position.xyz, 1.0);\n"
"  gl_Position  = vpmatrix * world;\n"
"}\n";

static const char *fshader_pointlight_src =
"#version 330\n"
"uniform mat4 vmatrix;\n"
"uniform mat4 ivpmatrix;\n"
"layout(std140) uniform pointlight {\n"
"  vec4  position;\n"
"  vec4  color;\n"
"} light;\n"
"uniform sampler2D tdepth;\n"
"uniform sampler2D tdiffuse;\n"
"uniform sampler2D tnormal;\n"
"uniform sampler2D tspecular;\n"
"layout(location = 0) out vec4 color;\n"
"void main () {\n"
"  vec2  texcoord = gl_FragCoord.xy / textureSize(tdepth, 0);\n"
"  float depth = texture(tdepth, texcoord).r;\n"
"  vec3  coord = (ivpmatrix * vec4(texcoord, depth/1000.0, 1.0)).xyz;\n"
"  float dist  = distance((light.position).xyz, coord);\n"
"  float luminance = light.color.a / (dist * dist);\n"
""
"  vec3 norm;\n"
"  norm.xy = texture(tnormal, texcoord).xy;\n"
"  norm.z  = sqrt(1.0 - dot(norm.xy, norm.xy));\n"
""
"  vec3 diff = texture(tdiffuse, texcoord).rgb;\n"
"  vec4 spec = texture(tspecular, texcoord);\n"
//"  color.rgb = light.color.rgb * (luminance * diff * dot(norm, vec3(0.0, 0.0, 1.0)) + luminance * spec.rgb * pow(dot(norm, vec3(0.0, 0.0, 1.0)), spec.a*255.0));\n"
"  color.rgb = coord;\n"
"  color.a   = 1.0;\n"
//"  color = vec4(light.color.rgb, 1.0);\n"
"}\n";

/* to decode rgbe emissive value: glow.rgb * exp2(glow.a * 16.0 - 8.0) */

static const char *vshader_tonemap_src = 
"#version 330\n"
"layout(location = 0) in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord * 2.0 - 1.0, 0.0, 1.0);\n"
"}\n";

static const char *fshader_tonemap_src = 
"#version 330\n"
"uniform sampler2D tcomposite;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"void main() {\n"
"  vec3 c = texture(tcomposite, ftexcoord).rgb;\n"
"  color = vec4(1.0 / (1.0 + exp(-8.0 * pow(c, vec3(0.8)) + 4.0)) - 0.018, 1.0);\n"
"}\n";

static const char *vshader_blit_src =
"#version 330\n"
"uniform vec2 size;\n"
"uniform vec2 offset;\n"
"layout(location = 0) in vec2 vcoord;\n"
"smooth out vec2 ftexcoord;\n"
"void main () {\n"
"  ftexcoord = vcoord;\n"
"  gl_Position = vec4(vcoord * size + offset, 0.0, 1.0);\n"
"}\n";

static const char *fshader_blit_src = 
"#version 330\n"
"uniform sampler2D image;\n"
"smooth in vec2 ftexcoord;\n"
"layout(location = 0) out vec4 color;\n"
"void main() {\n"
"  color = texture(image, ftexcoord);\n"
"}\n";

