<table class="ui compact table">
  <thead>
    <tr>
      <th>id</th>
      <th>name</th>
      <th>stat(r/p/d)</th>
      <th>replica</th>
      <th>step</th>
    </tr>
  </thead>
  <tbody>
    {{#jobs}}
    <tr>
      <td><a href="#" onclick="handle_job_id_click('{{jobid}}')">{{jobid}}</a></td>
      <td>{{desc.name}}</td>
      <td>{{running_num}}/{{pending_num}}/{{deploying_num}}</td>
      <td>{{desc.replica}}</td>
      <td>{{desc.deploy_step}}</td>
    </tr>
    {{/jobs}}
   </tbody>
</table>
