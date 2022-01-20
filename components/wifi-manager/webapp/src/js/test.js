let sd = {};
let rf=false;
function getStatus() {
        const config = {};
        $(`#valuesTable input:text,  #valuesTable input:checked`).each(function(_index, entry) {
            switch (entry.attributes.dtype.value) {
                case 'string':
                    config[entry.name] = entry.value;
                    break;
                    case 'number':
                    config[entry.name] = Number(entry.value);
                break;
                case 'boolean':
                    config[entry.name] = entry.value=='true';
                break;
                default:
                    break;
            }
            
        });     
        return config;       
    }   

    // function getOptions(entry)  {
    //     let output='';
    //     for (const property in entry) { 
    //         output+=`<option value="${entry[property]}">${property}</option>`;
    //     }
    //     return output;
    // }
    function getRadioButton(entry){
        let output='';
        for (const property in sd[entry]) { 
            output+=`
            <div class="custom-control custom-radio">
            <input type="radio" class="custom-control-input" id="${entry}_${sd[entry][property]}" name="${entry}" value="${sd[entry][property]}" dtype='${typeof(sd[entry][property])}'>
            <label class="custom-control-label" for="${entry}_${sd[entry][property]}">${property}</label>
            </div>
            `;
        }
        return output;

    }

    var refreshStatus = function() {
        if(Object.keys(sd).length>0){
            if(rf) return;
            rf=true;
            $.getJSON('/status.json', function(data) {
                for (const property in data) {
                    const val = data[property];
                    let input = $(`#val_${property}, #valuesTable input[name="${property}"]`) ;
                    if(input.length>0){
                        if(input.is(':radio') ){
                            $(`#${property}_${val ?? 0}`).prop('checked',true);
                        }
                        else {
                            if(input.val() !==val && !input.is(":focus")){
                                input.val(val);
                            }
                        }

                    }
                    else {
                        
                        if(sd[property]){
                            $('#valuesTable').append(
                                `<tr><td>${property}</td>
                                <td >
                                ${getRadioButton(property)}
                                </td></tr>`);
                            $(`#${property}_${val ?? 0}`).prop('checked',true);
                        }
                        else {
                            $('#valuesTable').append(`<tr><td>${property}</td><td><input type='text' class='value form-control nvs' id="val_${property}" name='${property}' dtype='${typeof(val)}' ></input></td></tr>`);
                            $(`#val_${property}`).val(val);
                        }


                    }

                }
            })
            .fail(function() {

            })
            .done(function(){
                rf=false;
            });

        }
        else {
            $.getJSON('/statusdefinition.json', function(data) {
                sd=data;
            })
            .fail(function() {

            })
            .done(function(){
            });                                
        }

    }
    function pushStatus(){
        const data = {
                timestamp: Date.now(),
                status:  getStatus()
            };
            $.ajax({
                url: '/status.json',
                dataType: 'text',
                method: 'POST',
                cache: false,
                contentType: 'application/json; charset=utf-8',
                data: JSON.stringify(data),
            });
            console.log('sent config JSON with data:', JSON.stringify(data));
    }

    $(document).ready(function() {
        $('#save_status').on('click', function() {
            pushStatus();
        });            
        $( "#valuesTable" ).change(function() {
            pushStatus();
        });            
        
        setInterval(refreshStatus, 1000);
        $('svg >> symbol').each(function() { 
            $('#allIcons').append( `<svg style="fill:white; width:1.5rem; height: 1.5rem;">
            <use xlink:href="#${this.id}"></use>
          </svg>`);
         });
         
    }) ;
