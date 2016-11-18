#!/usr/bin/lua
module("luci.controller.creator.provisioning", package.seeall)

local ubus = require "ubus"
local jsonc = require("luci.jsonc")
local onboarding = require("creator.onboarding")
local provisioning = require("creator.provisioning")

local conn = ubus.connect();

function index()
    entry({"admin", "creator"}, call("creator_provisioning"), "Creator", 50).dependent=false
    entry({"admin", "creator", "provisioning"}, call("creator_provisioning"), "Provisioning", 11).dependent=false
    entry({"admin", "creator", "provisioning", "clicker_list"}, call("clicker_list"), nil, nil).dependent=false
    entry({"admin", "creator", "provisioning", "start_provisioning"}, call("start_provisioning"), nil, nil).dependent=false
    entry({"admin", "creator", "provisioning", "select_clicker"}, call("select_clicker"), nil, nil).dependent=false
    entry({"admin", "creator", "provisioning", "start_daemon"}, call("start_daemon"), nil, nil).dependent=false
    entry({"admin", "creator", "provisioning", "stop_daemon"}, call("stop_daemon"), nil, nil).dependent=false
end


function creator_provisioning()
    luci.http.prepare_content("text/html")
    local warning = nil;
    local isProvisioningDaemonRunning = true
    local status = conn:call("provisioning-daemon", "getState", {})
    if not onboarding.isOnboardingCompleted() then
        warning = "You must go through onboarding process first in order to start provisioning clickers"
    elseif status == nil then
        isProvisioningDaemonRunning = false
        warning = "Provisioning daemon is not runnig. Please start it first in order to start provisioning."
    end
    luci.template.render("creator_provisioning/provisioning", {warning=warning, isProvisioningDaemonRunning=isProvisioningDaemonRunning})

end

function clicker_list()
    luci.http.prepare_content("text/html")

    local response = provisioning.getProvisioningDaemonState()

    if response then
        luci.template.render("creator_provisioning/clicker_list", {clickers=response})
    end

end

function start_provisioning()
    local clickerID = tonumber(luci.http.formvalue("clickerID"))
    if clickerID == nil then
        luci.http.status (404, "Invalid clickerID")
    end

    local response = provisioning.startProvisioning()

end

function select_clicker()
    local clickerID = tonumber(luci.http.formvalue("clickerID"))
    if clickerID == nil then
        luci.http.status (404, "Invalid clickerID")
    end

    local response = provisioning.selectClicker(clickerID)


end

function start_daemon()
    if (provisioning.startProvisionignDaemon() == true) then
        luci.http.status(200, "OK")
    else
        luci.http.status(500, "Cannot start provisioning-daemon");
    end
end

function stop_daemon()
    if (provisioning.stopProvisioningDaemon() == true) then
        luci.http.status(200, "OK")
    else
        luci.http.status(500, "Cannot stop provisioning-daemon");
    end
end
