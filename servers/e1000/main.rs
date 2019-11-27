use crate::e1000::Device;
use crate::pci::Pci;
use resea::idl::{self, memmgr, network_device_client::nbsend_received};
use resea::prelude::*;
use resea::server::{publish_server};
use resea::utils::align_up;
use resea::PAGE_SIZE;

static MEMMGR_SERVER: Channel = Channel::from_cid(1);
static KERNEL_SERVER: Channel = Channel::from_cid(2);

struct Server {
    ch: Channel,
    listener: Option<Channel>,
    device: Device,
}

impl Server {
    pub fn new(ch: Channel, device: Device) -> Server {
        Server {
            ch,
            listener: None,
            device,
        }
    }
}

impl idl::network_device::Server for Server {
    fn get_macaddr(&mut self, _from: &Channel) -> Result<(u8, u8, u8, u8, u8, u8)> {
        let m = self.device.mac_addr();
        Ok((m[0], m[1], m[2], m[3], m[4], m[5]))
    }

    fn listen(&mut self, _from: &Channel, ch: Channel) -> Result<()> {
        assert!(self.listener.is_none());
        self.listener = Some(ch);
        Ok(())
    }

    fn transmit(&mut self, _from: &Channel, packet: Page) -> Result<()> {
        let data = packet.as_bytes();
        if packet.len() > data.len() {
            return Err(Error::InvalidArg);
        }

        self.device.send_ethernet_packet(data);
        Ok(())
    }
}

impl idl::server::Server for Server {
    fn connect(
        &mut self,
        _from: &Channel,
        interface: InterfaceId,
    ) -> Result<(InterfaceId, Channel)> {
        assert!(interface == idl::network_device::INTERFACE_ID);
        let client_ch = Channel::create()?;
        client_ch.transfer_to(&self.ch)?;
        Ok((interface, client_ch))
    }
}

impl resea::server::Server for Server {
    fn notification(&mut self, _notification: Notification) {
        self.device.handle_interrupt();
        let rx_queue = self.device.rx_queue();
        while let Some(pkt) = rx_queue.front() {
            if let Some(ref listener) = self.listener {
                let num_pages = align_up(pkt.len(), PAGE_SIZE) / PAGE_SIZE;
                let mut page =
                    match memmgr::call_alloc_pages(&MEMMGR_SERVER, num_pages) {
                        Ok(page) => page,
                        Err(_) => {
                            warn!("failed to allocate a page");
                            return;
                        }
                    };
                page.copy_from_slice(&pkt);
                let reply = nbsend_received(listener, page);
                match reply {
                    // Try later.
                    Err(Error::NeedsRetry) => (),
                    _ => {
                        rx_queue.pop_front();
                    }
                }
            } else {
                // No listeners. Drop the received packets.
                rx_queue.pop_front();
            }
        }
    }
}

#[no_mangle]
pub fn main() {
    info!("starting...");
    let pci = Pci::new(&KERNEL_SERVER);
    let ch = Channel::create().unwrap();
    let mut device = Device::new(&ch, &pci, &KERNEL_SERVER, &MEMMGR_SERVER);
    device.init();

    info!("initialized the device");
    info!("MAC address = {:x?}", device.mac_addr());

    let mut server = Server::new(ch, device);
    publish_server(idl::network_device::INTERFACE_ID, &server.ch).unwrap();
    info!("ready");
    serve_forever!(&mut server, [server, network_device]);
}
