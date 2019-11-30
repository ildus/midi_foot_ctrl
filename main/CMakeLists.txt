set(COMPONENT_SRCS
	"gatt.c"
	"midi.c"
	"http.c"
)
set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_EMBED_TXTFILES index.html components.js pure.css)

register_component()
