triac.net: triac.sch
	lepton-netlist -g spice-sdb -o triac.net triac.sch

triac: triac.net triac.inc
	ngspice triac.net

snubber.net: snubber.sch
	lepton-netlist -g spice-sdb -o snubber.net snubber.sch

snubber: snubber.net snubber.inc
	ngspice snubber.net

clean:
	rm -f triac.net snubber.net *~ symbols/*~

.PHONY: clean
