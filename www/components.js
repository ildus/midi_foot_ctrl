let tmpl = document.querySelector('#x-action-select-template');
customElements.define('action-select', class extends HTMLElement {
    constructor() {
        super();
        this.shadow = this.attachShadow({mode: 'open'});
        this.shadow.innerHTML = `
            <link rel="stylesheet" href="https://unpkg.com/purecss@1.0.1/build/pure-min.css" integrity="sha384-oAOxQR6DkCoMliIh8yFnu25d7Eq/PHS21PClpwjOTeU2jRSq11vu66rf90/cZr47" crossorigin="anonymous">
            <slot name="controls"></slot>
        `;
    }
    connectedCallback() {
        this.appendChild(tmpl.content.cloneNode(true));
        let self = this;
        let legend = this.querySelector("legend");
        legend.innerHTML = this.getAttribute("name");

        let form = this.querySelector("form");
        let form_data = "";

        function update_form_values() {
            let kvpairs = [];

            for (var i = 0; i < form.elements.length; i++ ) {
                let e = form.elements[i];
                if (e.value !== undefined)
                    kvpairs.push(e.name + "=" + e.value);
            }
            form_data = kvpairs.join("&");
            self.setAttribute("value", form_data);
        }
        update_form_values();

        let select = form.querySelector("select");
        select.addEventListener("change", update_form_values);

        let input1 = form.querySelector('input[name="d1"]');
        input1.addEventListener("change", update_form_values);

        let input2 = form.querySelector('input[name="d2"]');
        input2.addEventListener("change", update_form_values);
    }
});

window.onload = function () {
    let main_btn = document.getElementById("save");
    main_btn.addEventListener("click", function () {
        let selects = document.getElementsByTagName("action-select");
        let value = "";

        // collect values
        for (let i = 0; i < selects.length; i++) {
            let item = selects[i];
            value += item.getAttribute("id") + ":" + item.getAttribute("value") + "\n";
        }

        // save configuration
        fetch("/configure", {
            method: "POST",
            body: value
        }).then(res => {
            console.log("Request complete! response:", res);
        });
    });
};
