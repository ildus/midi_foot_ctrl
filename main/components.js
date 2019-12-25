/*jshint esversion: 6 */
let tmpl = document.querySelector('#x-action-select-template');
let initialMap = {};

/* parse vals */
if (initial_values)
{
    let lines = initial_values.split('\n');
    lines.forEach(line => {
        if (line.length === 0)
            return;

        let parts = line.split(':');
        let id = parts[0];
        let vals = parts[1].split('&');

        initialMap[id] = [];
        let curval = {};
        vals.forEach(val => {
            let keyval = val.split('=');
            curval[keyval[0]] = keyval[1];
            if (keyval[0] == "d2")
            {
                initialMap[id].push(curval);
                curval = {};
            }
        });
    });
}

function setup_inputs(parent_el, callback, vals)
{
    let action_select = parent_el.querySelector('select[name="action"]');
    action_select.addEventListener("change", callback);

    let ch_select = parent_el.querySelector('select[name="channel"]');
    ch_select.addEventListener("change", callback);

    let input1 = parent_el.querySelector('input[name="d1"]');
    input1.addEventListener("change", callback);

    let input2 = parent_el.querySelector('input[name="d2"]');
    input2.addEventListener("change", callback);

    if (vals !== undefined)
    {
        action_select.value = vals.action;
        ch_select.value = vals.channel;
        input1.setAttribute("value", vals.d1);
        input2.setAttribute("value", vals.d2);
    }
}

function extend_form(form, callback, vals)
{
    let fields = form.querySelector(".fields");
    let cloned = fields.cloneNode(true);
    let inner_add = cloned.querySelector('button.add');
    inner_add.innerHTML = '&minus;';

    let inner_play = cloned.querySelector('button.play');
    inner_play.setAttribute('disabled', true);

    cloned.querySelector('button.add').addEventListener('click', function (e) {
        e.preventDefault();
        cloned.remove();
        callback();
    });

    setup_inputs(cloned, callback, vals);
    form.appendChild(cloned);
    callback();
}

customElements.define('action-select', class extends HTMLElement {
    constructor() {
        super();
        this.shadow = this.attachShadow({mode: 'open'});
        this.shadow.innerHTML = `
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
                if (e.name == 'action' || e.name == 'd1' || e.name == 'd2' || e.name == 'channel')
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
        setup_inputs(form, update_form_values, vals[0]);

        if (vals.length > 1)
        {
            for (let i=1; i < vals.length; i++)
                extend_form(form, update_form_values, vals[i]);
        }

        update_form_values();
        form.querySelector("button.play").addEventListener("click", function (e) {
            e.preventDefault();

            fetch("/configure", {
                method: "POST",
                body: "play:" + get_values(),
            }).then(res => {
                if (res.ok)
                    show_notify("Played");
                else
                    res.text().then(text => {
                        show_notify("Error: " + text, true);
                    });
            }).catch(res => {
                show_notify("Error: " + res, true);
            });
        });

        form.querySelector("button.add").addEventListener("click", function (e) {
            e.preventDefault();
            extend_form(form, update_form_values);
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

        // collect values
        let value = "";
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
            if (res.ok)
                show_notify("saved");
            else
                res.text().then(text => {
                    show_notify("Error: " + text, true);
                });

            main_btn.removeAttribute("disabled");
        }).catch(res => {
            show_notify("Error: " + res, true);
            main_btn.removeAttribute("disabled");
        });
    });
};
