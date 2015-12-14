<h4>Cluster Cpu Realtime Statistics</h4>

<div class="ui four statistics">
  <div class="statistic">
    <div class="value">
      {{cpu.cpu_total}}
    </div>
    <div class="label">
      Total Cpu 
    </div>
  </div>
  <div class="statistic">
    <div class="value">
      {{cpu.cpu_used}}
    </div>
    <div class="label">
      Total Cpu Used
    </div>
  </div>
  <div class="statistic">
    <div class="value">
       {{cpu.cpu_assigned}}
    </div>
    <div class="label">
      Total Cpu Assigned
    </div>
  </div>
 <div class="red statistic">
    <div class="value">
       {{cpu.overused}}
    </div>
    <div class="label">
       Total Cpu Overused
    </div>
  </div>
</div>
