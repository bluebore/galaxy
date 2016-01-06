<table class="table table-striped">
       <thead>
          <tr> 
            <th>pod</th>
            <th>agent</th>
            <th>internal_error</th>
            <th>error</th>
            <th>cmd</th> 
            <th>deploy</th> 
            <th>main</th> 
            <th>exit_code</th> 
            <th>ftime</th> 
          </tr>
       </thead> 
        {{#tasks}}
           <tr>
               <td>{{pod_id}}</td>
               <td><a href="{{agent_addr}}">{{agent_addr}}</a></td>
               <td>{{internal_error}}</td>
               <td>{{error}}</td>
               <td>{{cmd}}</td>
               <td>{{deploy}}</td>
               <td>{{main}}</td>
               <td>{{exit_code}}</td>
               <td>{{ftime}}</td>
           </tr>
        {{/tasks}}
 </table>

