let tmpl = document.querySelector('#x-action-select-template');
let initialMap = {};

/* parse vals */
if (initial_values)
{
    let lines = initial_values.split('\n');
    lines.forEach(line => {
        if (line.length == 0)
            return;

        let parts = line.split(':');
        let id = parts[0];
        let vals = parts[1].split('&')

        initialMap[id] = {};
        vals.forEach(val => {
            let keyval = val.split('=');
            initialMap[id][keyval[0]] = keyval[1];
        })
    });
}

customElements.define('action-select', class extends HTMLElement {
    constructor() {
        super();
        this.shadow = this.attachShadow({mode: 'open'});
        this.shadow.innerHTML = `
            <link rel="stylesheet" href="pure.css">
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

        function get_values() {
            let kvpairs = [];

            for (var i = 0; i < form.elements.length; i++ ) {
                let e = form.elements[i];
                if (e.name == 'action' || e.name == 'd1' || e.name == 'd2')
                    kvpairs.push(e.name + "=" + e.value);
            }
            form_data = kvpairs.join("&");
            return form_data;
        }

        function update_form_values() {
            form_data = get_values();
            self.setAttribute("value", form_data);
        }
        update_form_values();

        let vals = initialMap[this.getAttribute("id")];
        let select = form.querySelector("select");
        select.value = vals["action"];
        select.addEventListener("change", update_form_values);

        let input1 = form.querySelector('input[name="d1"]');
        input1.setAttribute("value", vals["d1"]);
        input1.addEventListener("change", update_form_values);

        let input2 = form.querySelector('input[name="d2"]');
        input2.setAttribute("value", vals["d2"]);
        input2.addEventListener("change", update_form_values);

        let playbtn = form.querySelector('button');
        playbtn.addEventListener("click", function (e) {
            e.preventDefault();

            fetch("/configure", {
                method: "POST",
                body: "play:" + get_values(),
            }).then(res => {
                show_notify("played");
            }).catch(res => {
                show_notify("Error: " + res, true);
            });
        });
    }
});

function show_notify(msg, error) {
	let notify = document.getElementById("notify");
	let notify_class = notify.classList;

	if (error)
		notify_class.add("alert-danger");
	else
		notify_class.add("alert-primary");

	notify.innerHTML = msg;
	notify.style.display = "inline";

	setTimeout(function () {
		notify.innerHTML = "";
		notify_class.remove("alert-danger");
		notify_class.remove("alert-primary");
		notify.style.display = "none";
	}, 2000);
}

function show_error(msg) {
	show_notify(msg, true);
}

window.onload = function () {
    let main_btn = document.getElementById("save");
    main_btn.addEventListener("click", function (e) {
        e.preventDefault();
        main_btn.setAttribute("disabled", true);
        let selects = document.getElementsByTagName("action-select");
        let value = "";

        // collect values
        for (let i = 0; i < selects.length; i++) {
            let item = selects[i];
            value += item.getAttribute("id") + ":" + item.getAttribute("value") + "\n";
        }

        // save configuration
        console.log(value);
        fetch("/configure", {
            method: "POST",
            body: value
        }).then(res => {
			show_notify("saved");
			console.log("Request complete! response:", res);
            main_btn.removeAttribute("disabled");
        }).catch(res => {
            show_notify("Error: " + res, true);
            main_btn.removeAttribute("disabled");
        });
    });
};
