.options noacct rshunt=1G
.tran 1u 100m
.measure tran P1 integ @R2[p] from=0m to=10m
.measure tran P2 integ @R2[p] from=10m to=28m
.measure tran P3 integ @R2[p] from=28m to=100m
.measure tran P4 integ @R2[p] from=0m to=100m
.probe vd(S1:3:4)
.probe vd(S1:1:2)

.model swmod sw vt=1

.control
save L, V(A), V(B)
save @R1[i], @R2[i], @R2[p]
save @L1[i]
run

.endc
