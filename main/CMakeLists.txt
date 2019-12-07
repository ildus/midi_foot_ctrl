set(COMPONENT_SRCS
	"main.c"
	"midi.c"
	"http.c"
	"control.c"
)
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_EMBED_TXTFILES index.html components.js pure.css)

register_component()
