.options noacct rshunt=1G
.tran 1u 40m
.measure tran P2 integ @R2[p] from=0m to=10m
.measure tran P3 integ @R3[p] from=0m to=10m
.measure tran P4 integ @R4[p] from=0m to=10m
.measure tran P7 integ @R7[p] from=0m to=10m
.probe I(X1,1) I(X1,3)
.probe I(X3,1) I(X3,3)
.probe I(X2,1) I(X4,1)
.probe I(X2,4) I(X4,4)
.probe I(X5,1)
.probe vd(X2:4:3)
.probe vd(X4:4:3)
.probe vd(X1:1:2)
.probe vd(X3:1:2)
.probe vd(X5:1:3)

.control
save Vm
save @R3[p] @R4[p] @R7[p]
save @R3[i] @R4[i] @R7[i]
save @C1[i]
run

*plot @R1[i] @R2[i] @R3[i]

* Pump power triac
* plot X1:A#branch*1000 vd_x1:a:k Vm max(0,deriv(abs(vd_x1:a:k))/1e6)

* Pump photoriac
* plot x2:4#branch*1000 vd_x2:4:3 vm max(0,deriv(abs(vd_x2:4:3))/1e6) X1:A#branch*1000

* Heater power triac
* plot X3:A#branch vd_x3:a:k/100 Vm/100 max(0,deriv(abs(vd_x3:a:k))/1e6)

* Heater photoriac
* plot x4:4#branch*1000 vd_x4:4:3 vm max(0,deriv(abs(vd_x4:4:3))/1e6) X3:A#branch

.endc
