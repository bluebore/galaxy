<div class="ui celled divided list">
  {{#stats}} 
  <div class="item" >
    <div class="content">
      <p class="header"> pod-{{id}}</p>
    </div>
    <div class="list" id="{{id}}">
        {{#logs}}
            level:{{level}} <br>
            agent:{{agent_addr}} <br>
            time:{{ftime}} <br>
            from:{{from}} <br>
            reason:{{reason}} <br>
            gc_dir:{{gc_dir}}<br>
           <a href="#" onclick="handle_pod_click('{{id}}', {{time}})" > detal </a> <br>
        {{/logs}}
    </div>
  </div>
  {{/stats}}
</div>
